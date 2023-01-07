# Generated from the first test:
| MWLC | state_change_a(Chicken) |  check_a(Chicken) |
| MWLC | state_change_a(Chicken) |  check_b(Chicken, Green) |
| MWLC | state_change_b(Chicken, Green) |  check_a(Chicken) |
| MWLC | state_change_b(Chicken, Green) |  check_b(Chicken, Green) |


# Generated from the second test:
| MWLC | state_change_a(Dog) |  check_b(Dog, Red) |

# Generated from the third test:
| C | state_change_a(Chicken) |  state_change_b(Chicken, Red) |  check_a(Chicken) |

# Generated from the fourth test:
| C | state_change_a(Dog) |  state_change_a(Chicken) |  check_b(Chicken, Green) |

# Generated from fifth test
| MWLC | state_change_a(Dog) | check_a(Dog) |
| MWLC | state_change_a(Dog) | check_a(Chicken) |