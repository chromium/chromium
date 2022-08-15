# Tests for all platforms.
# This test should generate 4 processed tests:
| MWLC | changes | checks |

# This test should generate 1 processed tests:
| MWLC | changes(Dog) | check_b(Dog, Red) |

# Tests only for ChromeOS
| C | state_change_a | state_change_b(Chicken, Red) | check_a |
| C | state_change_a(Dog) | state_change_a(Chicken) | check_b(Chicken, Green) |

# This test should generate 2 processed tests:
| MWLC | changes(Dog) | check_a(Animal::All) |