
| #Name | Arguments | OutputActions | ID | 
| --- | --- | --- | --- |
| #Check actions: |
| check_a | Animal |  | 1 |  | 
| check_b | Animal, Color |  | 2 |  | 
| #State change actions: |
| state_change_a | Animal |  | 3 |  | 
| state_change_b | AnimalLess, Color |  | 4 |  | 
| #Parameterized actions. |
| changes | Animal | state_change_a($1) & state_change_b($1, Green) | 5 |  | 
| checks |  | check_a(Chicken) & check_b(Chicken, Green) | 6 |  | 
