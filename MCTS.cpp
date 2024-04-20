#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include<vector>
#include<cmath>
#include<algorithm>

#ifndef _BOTZONE_ONLINE
//#define DEBUG
#endif

#ifdef DEBUG
#define DBGprint(...) printf(__VA_ARGS__)
#else
#define DBGprint(...)
#endif

using namespace std;

const int SIZE = 11;
const int boardedge = 10;

struct node//搜索树节点结构体 
{
	//考虑添加该节点是否有人胜利,节省的时间与已形成的树深度成正比。
	short x;//存洛子坐标 ，可以是 敌我双方的 。关于类型，会大批量产生变量的要特别注意类型，尽量节省空间
	short y;//因为如果存下整个对局中的棋盘 ，存不了几个，算是用时间换空间 .还有一种节省空间方法可以把类型换成char然后用位运算 
	unsigned int win_times;//结点的深度不同 ，分别代表 对手和己方的胜利次数 
	unsigned int visits;//4000*30>6万。   Q：or type unsigned short 够用吗？ 
	node* kids_array;//将节点的所有 孩子节点放在一个数组里 ，这个变量相当于 数组首指针 。这样设置是为了 取缔expand步 。
	//如果想采用expand模式，则可以设置 成员为vector<node*> children;
	short SIMidx;//配合取缔expand。此外可在随机模拟、ucb时兼任“（加上无用位置的）结点孩子数”或者说遍历的结束位置。
	short useless_end;//字面意思，从首到其的[)区间内为无用孩子（剪枝）。兼任有用孩子首index
	node* parent;//用于反向传播 
	node() :visits(0), win_times(0), useless_end(0), SIMidx(-1) {}//构造后总需要设置xy和parent。而孩子数组指针在模拟时分配数组空间,SIMidx==-1是未distribute的标识，traverse里胜利的情况可用其判断
};
void UCT(int& finalx, int& finaly);
node* traverse(node* root);
node* ucbchoice(node* n, double c);
void DFS(int x, int y);
bool bluewin();
bool redwin();
int simulate(node* n);
void distribute(node* n);
void backup(int leaf_win_times, node* leaf);
bool recoverBridge( int x, int y, int& new_x, int& new_y);
void choosekids(int expandpointx[], int expandpointy[]);
bool uselessJudge(int x, int y); int useless_blance = 1;//2?
char getcolor(int x, int y);

int* input;
int n;
char mycolor;
int MCTSboard[SIZE][SIZE] = { 0 };
int MCTSoccupy = 0;
int main()
{
#ifndef _BOTZONE_ONLINE
	freopen("in.txt", "r", stdin);
#endif // !_BOTZONE_ONLINE
	scanf("%d", &n);
	input = new int[4 * n - 2];

	int* p = input;
	int x, y;
	//录入首个也是 至少的一个坐标 
	scanf("%d %d", &x, &y);
	*(p++) = x; *(p++) = y;
	if (x != -1) {
		MCTSboard[x][y] = -1;	//对方1,2
		MCTSoccupy++;
	}

	for (int i = 1; i < n; i++) {
		scanf("%d %d", &x, &y);
		*(p++) = x; *(p++) = y;
		MCTSboard[x][y] = 1;	//我方
		MCTSoccupy++;

		scanf("%d %d", &x, &y);
		*(p++) = x; *(p++) = y;
		MCTSboard[x][y] = -1;	//对方
		MCTSoccupy++;
	}

	int new_x, new_y;
	DBGprint("\n输入的最后一个坐标 %d %d", input[4 * n - 4], input[4 * n - 3]);
	if (input[4 * n - 4] == -1) {//最后一个坐标是-1 -1 	游戏刚开始qie我方为先手即红方
		new_x = 1; new_y = 2;
	}

	else {
		if (MCTSboard[7][3] == 0) {
			new_x = 7; new_y = 3;
		}
		else if (MCTSboard[3][7] == 0) {
			new_x = 3; new_y = 7;
		}
		else {
			//记录其他信息 
			if (input[0] == -1) {//我方为先手即红方
				//ori_emptynums = SIZE * SIZE - 2 * (n - 1);
				mycolor = 'R';
			}
			else {
				//ori_emptynums = SIZE * SIZE - 2 * n - 1;
				mycolor = 'B';
			}
			if (recoverBridge( input[4 * n - 4], input[4 * n - 3], new_x, new_y)) {

			}
			else {
				UCT(new_x, new_y);
			}
		}
	}

	delete[] input;
	DBGprint("\n最终决策结果 :");
	cout << new_x << ' ' << new_y << endl;
	return 0;
}

int threshold = 0.980 * (double)CLOCKS_PER_SEC;
int start_time = clock();//程序一开始的时刻 
int current_time;
#ifdef DEBUG
bool onceprint = false;
#endif // DEBUG

int rollout_times = 20;//可以设置随层渐增，即增加多回合模拟的权重， 而且 在节点数较多时平衡总时间 。
int expand_count = 0;
int xofkid[121] = { 0 }, yofkid[121] = { 0 };
struct coordinate
{
	int x; int y;
	coordinate() :x(-1), y(-1) {}
	coordinate(int i, int j) :x(i), y(j) {}
};
void UCT(int& finalx, int& finaly) {
	node* virtualroot = new node; virtualroot->x = virtualroot->y = -1; virtualroot->parent = NULL;//因为节点 只记录模拟的落子位置 ，所以root x,y无意义。
	choosekids(xofkid, yofkid);
	int expand_count_copy= expand_count;
	distribute(virtualroot);
#ifdef DEBUG
	int maxd = 1;
#endif // !
	//traverse是每向下搜索一层 填充无用位置 ，最后搜索叶子节点分配并无用位置 ，模拟期间再填充无用位置
	while (1) {
		//bool bluewin = false, redwin = false;
		node* leaf = traverse(virtualroot);//叶子节点是要从其局面开始随机模拟的节点 。或
#ifdef DEBUG
		if (!leaf) printf("\nleaf为空");
#endif // DEBUG
		bool Bwin = bluewin();
		bool Rwin = redwin();
		if (!Bwin && !Rwin) {
			if (current_time - start_time < threshold) {
				distribute(leaf);//预先为该节点拓展出所有子节点 ，但都还未模拟过所以模拟次数都是零 。
				int win_times = simulate(leaf);//不管是敌方回合还是我方回合 ，记录其模拟的 获胜次数 
				if (current_time - start_time < threshold)
					backup(win_times, leaf);
			}
		}
		else {
			bool myturn = MCTSboard[leaf->x][leaf->y] == 1;//获胜结点是否为我方落子
			if (myturn && mycolor == 'B' && Bwin || myturn && mycolor == 'R' && Rwin || !myturn && mycolor == 'R' && Bwin || !myturn && mycolor == 'B' && Rwin)
				backup(rollout_times, leaf);
			else backup(0, leaf);
		}
		//为下次循环初始化 MCTSboard MCTSoccupy
		memset(MCTSboard, 0, sizeof(MCTSboard));
		MCTSoccupy = 0;

		if (input[0] != -1) {
			MCTSboard[1][2] = -1;//1，2
			MCTSoccupy++;
		}
		int idx = 0;
		idx = 2;
		for (int i = 1; i < n; i++) {
			int x = input[idx++]; int y = input[idx++];
			MCTSboard[x][y] = 1;
			MCTSoccupy++;

			x = input[idx++]; y = input[idx++];
			MCTSboard[x][y] = -1;
			MCTSoccupy++;
		}
		expand_count = expand_count_copy;
		current_time = clock();//保险起见且位于最外层循环相对代价小
		if (current_time - start_time > threshold) break;
	}

	node* tmp = (virtualroot->kids_array) + (virtualroot->useless_end); node* end = virtualroot->kids_array + (virtualroot->SIMidx);//节省运算时间 
	node* result = tmp;
	int maxvisits = 0;
	for (; tmp != end; tmp++) {
		if (tmp->visits > maxvisits || tmp->visits == maxvisits && tmp->win_times > result->win_times) {
			maxvisits = tmp->visits;
			result = tmp;
		}
	}
#ifdef DEBUG
	if (!result) printf("\nucb结果怎会==NULL");
#endif // 
	finalx = result->x; finaly = result->y;
}

void maintain_kidsarr(int x,int y){
	for(int i=0;i< expand_count;i++)
		if (xofkid[i] == x) 
			if (yofkid[i] == y) {
				expand_count--;
				xofkid[i] = xofkid[expand_count];
				yofkid[i] = yofkid[expand_count];
				xofkid[expand_count] = x;
				yofkid[expand_count] = y;
			}		
}
node* traverse(node* root) {//从根节点开始遍历找出一个叶子节点,同时在棋盘上落子也就是在假装结点存的是棋盘
	node* now = root;
	bool myturn = false;//root的
	//int kidnum_of_now = 121 - MCTSoccupy;
	int kidnum_of_now = expand_count;
	do {//根节点肯定没分出胜负，那就省一次判断
		for (int k = now->useless_end - 1; k >= 0; k--)//额外填补无用位置,不会改变填补前局面的未决胜负。 
		{
			MCTSboard[now->kids_array[k].x][now->kids_array[k].y] = useless_blance;
			useless_blance = -useless_blance;
			maintain_kidsarr(now->kids_array[k].x, now->kids_array[k].y);
		}
		MCTSoccupy += now->useless_end;
		kids_num-=now->useless_end;
		if (now->SIMidx == kidnum_of_now ){//在一个节点的所有子节点都模拟过，即该结点已完全拓展了时，向下一层迭代 
			now = ucbchoice(now, 2.0);
			myturn = !myturn;
			kidnum_of_now--;
			if (myturn)
				MCTSboard[now->x][now->y] = 1;
			else
				MCTSboard[now->x][now->y] = -1;
			MCTSoccupy++;
			maintain_kidsarr(now->x, now->y);
			if (now->SIMidx == -1) {//now局面一定有胜利
				break;
			}
			else;//肯定没人赢不用检查了
			current_time = clock();
			}
		else if (now->SIMidx < kidnum_of_now+now->useless_end ) {//不是一个个拓展子节点 ,而是提前设置好所有子节点，只是子节点一开始模拟次数都是零,仍然需要一个个模拟 ，SIMidx索引准备模拟的子节点
			(now->SIMidx)++;
			now = &(now->kids_array[now->SIMidx - 1]);
			myturn = !myturn;
			//kidnum_of_now--;此处无用
			if (myturn)
				MCTSboard[now->x][now->y] = 1;
			else
				MCTSboard[now->x][now->y] = -1;
			maintain_kidsarr(now->x, now->y);
			MCTSoccupy++;
			//返回一个未模拟节点。存在这种情况：如果返回后时限已到便无法模拟则索引值确实是错的，但是程序要结束了这个索引值接下来也没有用了（注意只有索引值错了返回的节点是没错的 ）		
			return now;//其无用位置虽现在未填，但若随机模拟，随机模拟时会填
		}
		
		else { cout << "\nnow->SIMidx > kidnum_of_now+now->useless_end)"; break; }
	} while (current_time - start_time < threshold);//Q：如果树中的节点会特别多 ，也可以考虑终止条件设为棋盘满没满.
	return now;//终止态结点。在if中break做的事不多，可以认为返回时刚检测了时间
}
int dx[6] = { 0,1,1,0,-1,-1 };
int dy[6] = { 1,0,-1,-1,0,1 };
bool dfs_terminal;
int sign;
char dfs_color;
bool dfstrail[SIZE][SIZE] = { false };//避免环、回头路、不同起点走一样的路
bool bluewin() {//给定任意阶段的棋盘 ，判断蓝方是否已经取胜。如果棋盘已满 ，必有一方获胜， 若蓝不胜则知红胜。
	dfs_color = 'B';
	memset(dfstrail, false, sizeof(dfstrail));

	if (mycolor == 'R')
		sign = -1;
	else sign = 1;
	dfs_terminal = false;
	for (int i = 0; i <= boardedge; i++) {
		if (MCTSboard[i][0] == sign)//start
			DFS(i, 0);
		if (dfs_terminal == true)
			break;
	}
	return dfs_terminal;
}
bool redwin() {
	dfs_color = 'R';
	memset(dfstrail, false, sizeof(dfstrail));

	if (mycolor == 'R')
		sign = 1;
	else sign = -1;
	dfs_terminal = false;
	for (int i = 0; i <= boardedge; i++) {
		if (MCTSboard[0][i] == sign)
			DFS(0, i);
		if (dfs_terminal)
			break;
	}
	return dfs_terminal;
}
void DFS(int x, int y) {
	if (dfs_color == 'B') {
		if (y == boardedge) {
			dfs_terminal = true;
			return;
		}
	}
	else if (dfs_color == 'R') {
		if (x == boardedge) {
			dfs_terminal = true;
			return;
		}
	}
	for (int i = 0; i < 6; i++) {
		if (dfs_terminal == true) break;
		x = x + dx[i]; y = y + dy[i];
		if (x >= 0 && x <= boardedge && y >= 0 && y <= boardedge && MCTSboard[x][y] == sign && dfstrail[x][y] == false) {
			dfstrail[x][y] = true;
			DFS(x, y);
		}
		x = x - dx[i]; y = y - dy[i];//回溯
	}
}
node* ucbchoice(node* n, double c) {
	double max = 0.0; node* result = n->kids_array + (n->useless_end);
	node* tmp = result, * end = n->kids_array + (n->SIMidx);//节省运算时间.
	unsigned int n_visits = n->visits;
	for (; tmp != end; tmp++) {
		double UCB = (double)tmp->win_times / (double)tmp->visits + c * sqrt(log(n_visits) / (double)tmp->visits);
		if (UCB > max) {
			max = UCB;
			result = tmp;
		}
	}
	return result;
}
int simulate(node* leaf) {

	//开始模拟
	int emptynums = 121 - MCTSoccupy;
	int bluewin_cnt = 0;
#ifdef DEBUG
	int Rwincnt = 0;
#endif // DEBUG

	int remainSIM = rollout_times;
	coordinate* randomarr = new coordinate[emptynums];
	int idx = 0;
	for (short i = 0; i <= boardedge; i++)
		for (short j = 0; j <= boardedge; j++) {
			if (MCTSboard[i][j] == 0) {
				randomarr[idx].x = i;
				randomarr[idx].y = j;
				idx++;
			}
		}
	bool myturn = MCTSboard[leaf->x][leaf->y] == 1;
	int pawn;//模拟用棋子
	if (myturn) pawn = -1;
	else pawn = 1;
	while (remainSIM-- && current_time - start_time < threshold) {//如果模拟次数少且其余操作省时，可以把时间判断移除循环。此外如果模拟次数多的话，中断的影响需要处理的。
		srand(unsigned(7 * time(NULL)));
		random_shuffle(randomarr, randomarr + emptynums);
		int remain_nums = emptynums;
		while (remain_nums--) {//判断之后对齐下标
			int x = randomarr[remain_nums].x, y = randomarr[remain_nums].y;
			if (!uselessJudge(x, y))
			{
				MCTSboard[x][y] = useless_blance;
				useless_blance = -useless_blance;
				//MCTSoccupy++  不写也可以 
			}
			else {
				MCTSboard[x][y] = pawn;
				pawn = -pawn;
				//MCTSoccupy++  不写也可以 
			}
		}
		if (bluewin())bluewin_cnt++;
#ifdef DEBUG
		if (redwin())Rwincnt++;
		if (Rwincnt + bluewin_cnt + remainSIM != rollout_times) printf("\n判胜error");
#endif // DEBUG

		current_time = clock();
	}
	delete[] randomarr;
	//模拟结束
	if (remainSIM > -1) return rollout_times / 2;//返回个无效数据即可
	if (myturn && mycolor == 'B' || !myturn && mycolor == 'R') return bluewin_cnt;//无论该节点为己方敌方，返回其获胜局数 
	else return rollout_times - bluewin_cnt;
}
#ifdef DEBUG
int cnt = 0;
#endif // DEBUG
void distribute(node* n) {//相当于一次性拓展.
	int kids_num = expand_count;
	//n->kids_array = new node[121 - MCTSoccupy];
	n->kids_array = new node[kids_num];
	//node* tmp = (n->kids_array) + (121 - MCTSoccupy - 1);//节省运算时间 
	node* tmp = (n->kids_array) + (kids_num - 1);//节省运算时间 
	//for (int i = 0; i <= boardedge; i++)//不用怀疑，找空位对棋盘遍历就是最轻松高效的方法
	//	for (int j = 0; j <= boardedge; j++) {
	//			if (MCTSboard[i][j] == 0) {
	//			}
	//		}
	//	}
	while (kids_num > 0) {
		int i = xofkid[kids_num - 1], j = yofkid[kids_num - 1];
			if (!uselessJudge(i, j)) {
				n->kids_array[n->useless_end].x = i;
				n->kids_array[n->useless_end].y = j;
				n->kids_array[n->useless_end].parent = n;
				n->useless_end++;
			}
			else {
				tmp->x = i;
				tmp->y = j;
				tmp->parent = n;
				tmp--;
			}
		kids_num--;
	}
	tmp++;
#ifdef DEBUG
	if (tmp != n->kids_array + n->useless_end) printf("\n孩子安排错乱,n->useless_end=%d,tmp->y==%hd,n->kids_array[n->useless_end].y=%hd", n->useless_end, tmp->y, n->kids_array[n->useless_end].y);
#endif // DEBUG
	n->SIMidx = n->useless_end;
#ifdef DEBUG
	cnt++;
	//cout << "\n已经分配" << cnt << "个结点";
#endif // DEBUG
}
void backup(int leaf_win_times, node* leaf) {//模拟点的数据一同更改
	int win_times = leaf_win_times;
	for (node* p = leaf; p; p = p->parent) {
		p->win_times += win_times;
		p->visits += rollout_times;
		win_times = rollout_times - win_times;//每一层翻转获胜 与失败次数 
	}
}
bool uselessJudge(int x, int y) {
	int my_color_num = 0;
	int enemy_color_num = 0;
	int empty = 0;
	int invalid = 0;

	for (int i = 0; i < 6; i++) {
		x = x + dx[i]; y = y + dy[i];
		if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
			switch (MCTSboard[x][y]) {
			case 1:
				my_color_num++;
				break;
			case -1:
				enemy_color_num++;
				break;
			case 0:
				empty++;
				break;
			default:
				break;
			}
		}
		else {
			invalid++;
		}
		x = x - dx[i]; y = y - dy[i];
	}
	if (invalid == 4) {
		if (x == 0) {
			if (my_color_num == 1 && enemy_color_num == 1 && getcolor(x, y + 1) == 'R')return false;
			return true;
		}
		else {
			if (my_color_num == 1 && enemy_color_num == 1 && getcolor(x, y - 1) == 'R')return false;
			return false;
		}
	}
	else if (invalid == 3) {
		if (x == 0) {
			if (enemy_color_num == 1 && my_color_num == 1 && getcolor(x, y - 1) == 'R' && getcolor(x + 1, y) == 'B')return false;
			if (enemy_color_num == 3 || my_color_num == 3)return false;
			return true;

		}
		else {
			if (enemy_color_num == 1 && my_color_num == 1 && getcolor(x, y + 1) == 'R' && getcolor(x - 1, y) == 'B')return false;
			if (enemy_color_num == 3 || my_color_num == 3)return false;
			return true;
		}
	}
	else if (invalid == 2) {
		int flag_my = 0;
		int flag_enemy = 0;
		if (empty == 4 || 3 || 2) {
			return true;
		}
		else if (empty == 1) {
			if (my_color_num == 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
						if (MCTSboard[x][y] == 1) {
							int p = i - 1;
							int p2 = i + 1;
							if (p == -1) {
								p = 5;
							}
							if (p2 == 6) {
								p2 = 0;
							}
							if (x - dx[i] + dx[p] >= 0 && x - dx[i] + dx[p] <= 10 && MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1)return false;
							if (x - dx[i] + dx[p2] >= 0 && x - dx[i] + dx[p2] <= 10 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1)return false;
						}

					}
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
			if (enemy_color_num == 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
						if (MCTSboard[x][y] == -1) {
							int p = i - 1;
							int p2 = i + 1;
							if (p == -1) {
								p = 5;
							}
							if (p2 == 6) {
								p2 = 0;
							}
							if (x - dx[i] + dx[p] >= 0 && x - dx[i] + dx[p] <= 10 && MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1)return false;
							if (x - dx[i] + dx[p2] >= 0 && x - dx[i] + dx[p2] <= 10 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == -1)return false;
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
		}
		else if (empty == 0) {
			if (my_color_num == 4 || enemy_color_num == 4)return false;
			if (my_color_num == 3) {
				int flag_jud = 0;
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
						if (MCTSboard[x][y] == -1) {
							int p = i - 1;
							int p2 = i + 1;
							if (p == -1) {
								p = 5;
							}
							if (p2 == 6) {
								p2 = 0;
							}
							if (x - dx[i] + dx[p] >= 0 && x - dx[i] + dx[p] <= 10 && MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1)flag_jud++;
							if (x - dx[i] + dx[p2] >= 0 && x - dx[i] + dx[p2] <= 10 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1)flag_jud++;
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				if (flag_jud == 2)return false;
				return true;
			}
			if (enemy_color_num == 3) {
				int flag_jud = 0;
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
						if (MCTSboard[x][y] == 1) {
							int p = i - 1;
							int p2 = i + 1;
							if (p == -1) {
								p = 5;
							}
							if (p2 == 6) {
								p2 = 0;
							}
							if (x - dx[i] + dx[p] >= 0 && x - dx[i] + dx[p] <= 10 && MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1)flag_jud++;
							if (x - dx[i] + dx[p2] >= 0 && x - dx[i] + dx[p2] <= 10 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == -1)flag_jud++;
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				if (flag_jud == 2)return false;
				return true;
			}
			if (my_color_num == 2) {
				int flag_jud = 0;
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (x >= 0 && x <= 10 && y >= 0 && y <= 10) {
						if (MCTSboard[x][y] == 1) {
							int p = i - 1;
							int p2 = i + 1;
							if (p == -1) {
								p = 5;
							}
							if (p2 == 6) {
								p2 = 0;
							}
							if (x - dx[i] + dx[p] >= 0 && x - dx[i] + dx[p] <= 10 && MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1) {
								flag_jud++;
							}
							else {
								flag_jud++;
							}
							if (x - dx[i] + dx[p2] >= 0 && x - dx[i] + dx[p2] <= 10 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1) {
								flag_jud++;
							}
							else {
								flag_jud++;
							}
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				if (flag_jud == 2)return false;
				return true;
			}
		}

	}
	else if (invalid == 0) {
		int x2 = x;
		int y2 = y;
		if (empty >= 3)return true;
		if (empty <= 2) {
			if (my_color_num > 4 || enemy_color_num > 4)return false;
			if (my_color_num == 4 && enemy_color_num <= 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == -1 || MCTSboard[x][y] == 0) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 0 || MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1)return false;
						if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 0 || MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == -1)return false;
						return true;
					}
					x = x - dx[i]; y = y - dy[i];
				}
			}
			if (enemy_color_num == 4 && my_color_num <= 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 0 || MCTSboard[x][y] == 1) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 0 || MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1)return false;
						if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 0 || MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1)return false;
						return true;
					}
					x = x - dx[i]; y = y - dy[i];
				}
			}
			if (my_color_num == 3 && enemy_color_num == 1) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 0) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1) {
							p = p - 1;
							if (p == -1) {
								p = 5;
							}
							if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 0) {
								return false;
							}
						}
						if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == -1) {
							p2 = p2 + 1;
							if (p2 == 6) {
								p = 0;
							}
							if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 0) {
								return false;
							}
						}
						return true;
					}
					x = x - dx[i]; y = y - dy[i];
				}
			}
			if (enemy_color_num == 3 && my_color_num == 1) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 0) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1) {
							p = p - 1;
							if (p == -1) {
								p = 5;
							}
							if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 0) {
								return false;
							}
						}
						if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1) {
							p2 = p2 + 1;
							if (p2 == 6) {
								p = 0;
							}
							if (MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 0) {
								return false;
							}
						}
						return true;
					}
					x = x - dx[i]; y = y - dy[i];
				}
			}
			if (my_color_num == 3 && enemy_color_num == 3) {
				int flag_my = 0;
				int flag_enemy = 0;
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 1) {
						flag_my++;
						flag_enemy = 0;
					}
					else {
						flag_enemy++;
						flag_my = 0;
					}
					if (flag_my == 3 || flag_enemy == 3)return false;
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
			if (my_color_num == 2 && enemy_color_num == 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 0) {
						int p = i - 3;
						if (p < 0) {
							p = 6 + p;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 0)return false;
					}
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
			if (my_color_num == 3 && enemy_color_num == 2) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == 1) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == 1) {
							p = p - 2;
							if (p < 0) {
								p = 6 + p;
							}
							if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1)return false;
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
			if (my_color_num == 2 && enemy_color_num == 3) {
				for (int i = 0; i < 6; i++) {
					x = x + dx[i]; y = y + dy[i];
					if (MCTSboard[x][y] == -1) {
						int p = i - 1;
						int p2 = i + 1;
						if (p == -1) {
							p = 5;
						}
						if (p2 == 6) {
							p2 = 0;
						}
						if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == -1 && MCTSboard[x - dx[i] + dx[p2]][y - dy[i] + dy[p2]] == -1) {
							p = p - 2;
							if (p < 0) {
								p = 6 + p;
							}
							if (MCTSboard[x - dx[i] + dx[p]][y - dy[i] + dy[p]] == 1)return false;
						}
					}
					x = x - dx[i]; y = y - dy[i];
				}
				return true;
			}
		}

	}
	return true;
}
bool recoverBridge(int x, int y, int& new_x, int& new_y) {
	if (x - 1 >= 0 && y + 1 < SIZE && MCTSboard[x - 1][y] == MCTSboard[x][y + 1])
	{
		if (MCTSboard[x - 1][y + 1] == 0 && MCTSboard[x - 1][y] == 1)
		{
			new_x = x - 1;
			new_y = y + 1;
			return true;
		}
	}
	if (x - 1 >= 0 && y + 1 < SIZE && y - 1 >= 0 && MCTSboard[x - 1][y + 1] == MCTSboard[x][y - 1])
	{
		if (MCTSboard[x - 1][y] == 0 && MCTSboard[x - 1][y + 1] == 1)
		{
			new_x = x - 1;
			new_y = y;
			return true;
		}
	}
	if (x + 1 < SIZE && x - 1 >= 0 && y - 1 >= 0 && MCTSboard[x - 1][y] == MCTSboard[x + 1][y - 1])
	{
		if (MCTSboard[x][y - 1] == 0 && MCTSboard[x - 1][y] == 1)
		{
			new_x = x;
			new_y = y - 1;
			return true;
		}
	}
	if (x + 1 < SIZE && y - 1 >= 0 && MCTSboard[x][y - 1] == MCTSboard[x + 1][y])
	{
		if (MCTSboard[x + 1][y - 1] == 0 && MCTSboard[x][y - 1] == 1)
		{
			new_x = x + 1;
			new_y = y - 1;
			return true;
		}
	}
	if (x + 1 < SIZE && y + 1 < SIZE && y - 1 >= 0 && MCTSboard[x + 1][y - 1] == MCTSboard[x][y + 1])
	{
		if (MCTSboard[x + 1][y] == 0 && MCTSboard[x + 1][y - 1] == 1)
		{
			new_x = x + 1;
			new_y = y;
			return true;
		}
	}
	if (x + 1 < SIZE && x - 1 >= 0 && y + 1 >= 0 && MCTSboard[x + 1][y] == MCTSboard[x - 1][y + 1])
	{
		if (MCTSboard[x][y + 1] == 0 && MCTSboard[x + 1][y] == 1)
		{

			new_x = x;
			new_y = y + 1;
			return true;

		}
	}
	if (y == 0)
	{
		if (x - 1 >= 0 && MCTSboard[x - 1][y + 1] == 1 && MCTSboard[x - 1][y] == 0 && getcolor(x - 1, y + 1) == 'B')
		{

			new_x = x - 1;
			new_y = y;
			return true;

		}
		if (x + 1 < SIZE && MCTSboard[x][y + 1] == 1 && MCTSboard[x + 1][y] == 0 && getcolor(x - 1, y + 1) == 'B')
		{

			new_x = x + 1;
			new_y = y;
			return true;

		}

	}
	if (x == 0)
	{

		if (y - 1 >= 0 && MCTSboard[x + 1][y - 1] == 1 && MCTSboard[x][y - 1] == 0 && getcolor(x + 1, y - 1) == 'R')
		{

			new_x = x;
			new_y = y - 1;
			return true;

		}
		if (y + 1 < SIZE && MCTSboard[x + 1][y] == 1 && MCTSboard[x][y + 1] == 0 && getcolor(x + 1, y - 1) == 'R')
		{

			new_x = x;
			new_y = y + 1;
			return true;

		}

	}
	if (y == 10)
	{
		if (x + 1 < SIZE && MCTSboard[x + 1][y - 1] == 1 && MCTSboard[x + 1][y] == 0 && getcolor(x + 1, y - 1) == 'B')
		{

			new_x = x + 1;
			new_y = y;
			return true;

		}
		if (x - 1 >= 0 && MCTSboard[x][y - 1] == 1 && MCTSboard[x - 1][y] == 0 && getcolor(x + 1, y - 1) == 'B')
		{

			new_x = x - 1;
			new_y = y;
			return true;

		}

	}
	if (x == 10)
	{
		if (y + 1 < SIZE && MCTSboard[x - 1][y + 1] == 1 && MCTSboard[x][y + 1] == 0 && getcolor(x + 1, y - 1) == 'R')
		{

			new_x = x;
			new_y = y + 1;
			return true;

		}
		if (y - 1 >= 0 && MCTSboard[x - 1][y] == 1 && MCTSboard[x][y - 1] == 0 && getcolor(x + 1, y - 1) == 'R')
		{

			new_x = x;
			new_y = y - 1;
			return true;

		}
	}
	return false;
}
char getcolor(int x, int y) {
	if (MCTSboard[x][y] == 1 && mycolor == 'R' || MCTSboard[x][y] == -1 && mycolor == 'B')return 'R';
	else if (MCTSboard[x][y] == 1 && mycolor == 'B' || MCTSboard[x][y] == -1 && mycolor == 'R')return 'B';
	else if (MCTSboard[x][y] == 0) return 'N';//纯空白格
	//else if (MCTSboard[x][y] == 2) return 'O';//无用位置占用
	else return 'W';//意料之外的情况
}
void choosekids(int expandpointx[], int expandpointy[]) {
	int dist[11][11], qx[121], qy[121];  int max_expand = 2;
	int ql = 0, qr = -1;
	int begin_qr = qr;
	for (int i = 0; i < SIZE; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			dist[i][j] = -1;
		}
	}
	for (int i = 0; i < SIZE; i++)
	{
		for (int j = 0; j < SIZE; j++)
		{
			if (MCTSboard[i][j] == 1|| MCTSboard[i][j]==-1)
			{
				dist[i][j] = 0;
				qr++;
				qx[qr] = i;
				qy[qr] = j;
			}
		}
	}
	while (ql <= qr)
	{
		int x = qx[ql], y = qy[ql]; ql++;
		if (dist[x][y] == max_expand) break;
		for (int i = 0; i < 6; i++)
		{
			int tx = x + dx[i], ty = y + dy[i];
			if (tx >= 0 && tx < SIZE && ty >= 0 && ty < SIZE && dist[tx][ty] == -1)
			{
				dist[tx][ty] = dist[x][y] + 1;
				qr++, qx[qr] = tx, qy[qr] = ty;
				expandpointx[expand_count] = tx;
				expandpointy[expand_count] = ty;
				expand_count++;
			}
		}
	}
	return;
}
