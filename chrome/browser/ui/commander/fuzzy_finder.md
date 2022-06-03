# Commander Fuzzy Finder explainer

Most [Commander](README.md) sources use `FuzzyFinder` to select which available
commands match user input.

A `FuzzyFinder` instance is constructed with the user input, which we call the
_needle_. From then, callers call `Find()` with text to match, which we call
the _haystack_. `Find()` returns a score between 0.0 and 1.0 inclusive for
how well _needle_ matched _haystack_, and fills a provided out parameter,
`matched_ranges`, with the ranges of the characters of _haystack_ that match
_needle_.

Calls to `Find()` proceed through up to three layers, the last of which is the
most interesting and covered in depth below.
1. First, we check for certain special cases (single character _needle_, equal
length _needle_ and _haystack_ etc.) If the call matches one of the special
cases, we can handle the case without doing much work and return here.
2. Then, we run a simple   O(n) search (where _n_ is the length of
_haystack_) which we call "consecutive with gaps allowed". This checks that all
of _needle_ is present in _haystack_ (but, you guessed it, with gaps allowed!)
This served two purposes: providing a "good enough" result for large inputs,
and ensuring that all of _needle_ appears before spending the time running the
full algorithm. Most non-matches will return here.
3. A dynamic programming string search based on the command-line tool [fzf](https://github.com/junegunn/fzf/blob/master/src/algo/algo.go),
which is itself an adaptation of the [Smith-Waterman](https://en.wikipedia.org/wiki/Smith%E2%80%93Waterman_algorithm) ([original paper](https://dornsife.usc.edu/assets/sites/516/docs/papers/msw_papers/msw-042.pdf) very short, pretty readable) genetic sequence alignment
algorithm. This takes O(mn) time and space, so we only run this for _needles_
and _haystacks_ under a certain size.

## The matrix algorithm

      Aside: for simplicity, steps 1 and 2 above are performed in terms of UTF-16
      code units. The matrix algorithm, on the other hand, works in [code points](https://unicode.org/glossary/#code_point), which makes it more accurate for languages outside the [basic multilingual plane](https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane).

This is easiest to present in a worked example, so let's use
`needle` = "nwi"
`haystack` = "winter new window"

(this is an example where consecutive match with gaps would give us a bad
result: "wi\[n\]ter ne\[w\] w\[i\]ndow".

With `m = len(needle) = 3` and `n = len(haystack) = 17`, we will need the
following for scratch space:

#### A `n x m` score matrix:

|   | w | i | n | t | e | r |   | n | e | w |   | w | i | n | d | o | w |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| n |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| w |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| i |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |

where the cell at indices i,j means: the score for the substrings `needle[:j]`
and `haystack[:i]`. Scores in the score matrix are ints, because that's what
fzf used and they had good weights. We'll normalize at the end when reporting
the final score.

The exact calculation of the score for a match is arbitrary and subject to
change so we won't get into it in detail here. We *will* get into detail about
how the scores of previously filled cells affect the scores of subsequent cells.

#### A `n x m` "consecutive matrix" (not pictured, it looks exactly like the above when empty).

 The cell at i,j represents how many consecutive matched characters
of `needle` were found ending at this position. I guess formally, you could say:

If `n` is the value of the consecutive matrix at i, j, then for `x` in
1 through `n`: `needle[i - x] == haystack[j - x]`.

#### A length `n` word boundary array

(Constructing this is straightforward, so here's the final version)

|           | w | i | n | t | e | r |   | n | e | w |   | w | i | n | d | o | w |
|-----------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| boundary? | T | F | F | F | F | F | F | T | F | F | F | T | F | F | F | F | F |

This helps us track which bonuses for word boundaries, which are key for
matching acronyms.

#### A codepoint to code unit map

Since we're working in code points, we need this to generate matched ranges we
can use in the UI. For English and other BMP languages, this will be the
identity.

### Step 1: Fill the first row

First, we fill the first row of the score and consecutive matrices. This is
pretty straightforward. We iterate through `haystack`, and for each code unit:

#### Match

If it matches the first code unit in `needle`, set the corresponding cell
in the consecutive matrix to 1. Calculate a bonus for the match based on whether
it's on a word boundary (NB: this bonus is higher in the first row, since
presumably, the first character the user typed has very high salience).

#### No match

Otherwise, set the corresponding cell in the consecutive array to 0. As far as
the score, there's no points awarded for a match, but this might still be an
interesting position later based on how close it is to a previous match.

Given that, for a non-match, we set the score of this cell, to the score of the
cell to our immediate left, minus a penalty based on how far the gap from an
actual match to the current cell is. If there's no cell to the left or if the
resulting score is negative, use 0 instead.

After the first row is filled in, we have the score matrix:

|   | w | i | n  | t  | e  | r  |    | n  | e  | w  |    | w  | i  | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | 32 | 29 | 28 | 27 | 26 | 25 | 16 | 13 | 12 | 11 |
| w |   |   |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| i |   |   |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |

and consecutive matrix:

|   | w | i | n | t | e | r |   | n | e | w |   | w | i | n | d | o | w |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| n | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| w |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| i |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |


### Step 2: Fill in the rest

For the subsequent rows, each cell at i, j can represent:
- An extension of a gap. This is exactly the same as the non-match score for the
first row: the score of the cell to the left, minus a penalty. We call this the
`left_score`.
- A match (`needle[j] == haystack[i]`). For example, at `i == 11` and `j == 1`,
in our example, "w" matches. One way to look at this is that we had the prefixes

_needle_ "n"
_haystack_ "winter new "

and extended them both with "w". Given that, we score then match
(including bonuses) and add that to the  score of the prefixes: the cell
at i - 1, j - 1. We call this `diagonal_score`.

To recap: the left score means that we are extending a gap; that there's some
prefix of `haystack[:i]` that scores highly, and we're just piggybacking off of
it. We want to look at the left score even if we have a match, because there
could have been a previous match that was better. The diagonal score means that
we're extending the match; that this position should be bolded later when
we show _haystack_ to the user. Smith-Waterman also allowed for what I'll call
a "top score", which represents a gap in _needle_, but for our use cases, we
need all of _needle_ to match, so we're not interested.

The final score of the cell is max(0, left_score, diagonal_score)

If we choose the diagonal score, in *most cases* we fill the consecutive matrix
by incrementing the diagonal cell of the consecutive matrix (see source for
exceptions). If we choose the left score, then we don't consider this position
a match, so we're in a gap, and so the cell in the consecutive matrix is set to
1.

Now our matrices are filled!

Score:

|   | w | i | n  | t  | e  | r  |    | n  | e  | w  |    | w  | i  | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | 32 | 29 | 28 | 27 | 26 | 25 | 16 | 13 | 12 | 11 |
| w | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 45 | 42 | 51 | 48 | 47 | 46 | 45 | 44 |
| i | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 71 | 68 | 67 | 66 | 65 |

and consecutive:

|   | w | i | n | t | e | r |   | n | e | w |   | w | i | n | d | o | w |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| n | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| w | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| i | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |

### Step 3: Backtrack

Now that the score matrix is completely filled, we have a final score: the
highest score in the bottom row. If we just needed to rank, this would be
enough, but we also need to provide the positions of matches so that we can
highlight them for the user. For that, we need to backtrack through the matrix.

To do that, we start at a known match. The final score fits the bill.

|   | w | i | n  | t  | e  | r  |    | n  | e  | w  |    | w  | i      | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|----|----|----|----|----|--------|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | 32 | 29 | 28 | 27 | 26 | 25     | 16 | 13 | 12 | 11 |
| w | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 45 | 42 | 51 | 48     | 47 | 46 | 45 | 44 |
| i | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | >>71<< | 68 | 67 | 66 | 65 |

Based on how we built the score matrix, we know that for a match, we moved down
diagonally, so now we move up diagonally

|   | w | i | n  | t  | e  | r  |    | n  | e  | w  |    | w      | i  | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|----|----|----|----|--------|----|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | 32 | 29 | 28 | 27 | 26     | 25 | 16 | 13 | 12 | 11 |
| w | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 45 | 42 | >>51<< | 48 | 47 | 46 | 45 | 44 |
| i | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0      | 71 | 68 | 67 | 66 | 65 |

But this isn't necessarily the next position. If the score to the left is even
higher, *that* position is better. Practically, it means that we scan to the
left "edge", that is, scan left until the score to the left is no longer
higher than the current score.

At this point, since we could not have gotten to the current position by
extending a gap (since to use the left score, the current cell's score would
need to have been less than the left cell's), it must have been by moving
diagonally and that means that we've reached this row's match.

The first cell we look at in the second row is the right cell, but we do need
to do this in the top row:

|   | w | i | n  | t  | e  | r  |    | n  | e  | w  |        | w  | i  | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|----|----|----|--------|----|----|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | 32 | 29 | 28 | >>27<< | 26 | 25 | 16 | 13 | 12 | 11 |
| w | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 45 | 42     | 51 | 48 | 47 | 46 | 45 | 44 |
| i | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0      | 0  | 71 | 68 | 67 | 66 | 65 |
_not the right place, let's slide left_


|   | w | i | n  | t  | e  | r  |    | n      | e  | w  |    | w  | i  | n  | d  | o  | w  |
|---|---|---|----|----|----|----|----|--------|----|----|----|----|----|----|----|----|----|
| n | 0 | 0 | 16 | 13 | 12 | 11 | 10 | >>32<< | 29 | 28 | 27 | 26 | 25 | 16 | 13 | 12 | 11 |
| w | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0      | 0  | 45 | 42 | 51 | 48 | 47 | 46 | 45 | 44 |
| i | 0 | 0 | 0  | 0  | 0  | 0  | 0  | 0      | 0  | 0  | 0  | 0  | 71 | 68 | 67 | 66 | 65 |
_going left doesn't increase the score, this is the place!_



We repeat this process all the way up and including the top row. Now we have all
of the positions. The rest (fixing up the positions into ranges, normalizing the
score, etc.) is just grunt work.
