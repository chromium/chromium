# Score normalizer

The score normalizer is a heuristic algorithm that attempts to map a incoming
stream of numbers drawn from some unknown distribution to a uniform
distribution. This allows us to directly compare scores produced by different
underlying distributions, namely, by different search providers.

## Overview and terminology

The input to the normalizer is a stream of scores, `x`, drawn from an
underlying continuous distribution, `P(x)`. Its goal is to learn a function
`f(x)` such that `f(x ~ P)` is a uniform distribution over `[0,1]`.

## Reservoir sampling

Reservoir sampling is a method for computing `f`. Its simplest form is:

- Take the first `N` (eg. 100) scores as a 'reservoir'.
- Define `f(x) = sum(y < x for y in reservoir) / N`

In other words, we take the first `N` scores as a reservoir, and then
normalize a new score by computing its quantile within the reservoir.

We've constructed `f` by choosing a series of dividers (the reservoir scores)
which we hope are a representative sample of `P`. Consequently, a new `x ~ P`
will fall into any 'bucket' between two dividers with equal probability. So,
the index of the bucket x falls into is the quantile of x within `P`. Thus,
with some allowance for the fact that f is discrete, `f(x ~ P) ~= U(0,1)`.

The drawback of this is that the reservoir needs to be quite large before
this is effective.

## Bin-entropy reservoir sampling

We're going to trade off some accuracy for efficiency, and build on the core
idea of reservoir sampling. We will store 'bins' that cover the real line, and
try to keep the number of elements we've seen in each bin approximately equal.
Then we can compute `f(x)` by using the bin index of `x` as a quantile.

__Data structure__. We store a vector of N 'bins', each one with a count and
a lower divider. The bottom bin always has a lower divider of negative
infinity. `bins[i].count` roughly tracks the number of scores we've seen
between `bins[i-1].lower_divider` and `bins[i].lower_divider`.

__Algorithm overview.__ Each time a score is recorded, we increment the count
of the appropriate bin. Then, we propose a repartition of the bins as follows:

- Split the bin corresponding to the new score into two bins, divided by the
  new score. Halve the counts of the old bin into the two new bins.

- Merge together the two smallest contiguous bins, so that the total number of
  bins stays equal.

We accept this repartition if it increases the evenness across all bins, which
we measure using entropy. For simplicity, we prevent the merged bins from
containing either of the split bins.

__Entropy.__ Given a collection of bins, its entropy is:

```
H(p) = -sum( bin.count/total * log(bin.count/total) for bin in bins )
```

Entropy has two key properties:
1. The discrete distribution with maximum entropy is the uniform distribution.
2. Entropy is convex.

As such, we can make our bins as uniform as possible by seeking to maximize
entropy. This is implemented by only accepting a repartition if it increases
entropy.

__Fast entropy calculations.__ Suppose we are considering a repartition that:
 - splits bin `s` into `s_l` and `s_r`,
 - merges bins `m_l` and `m_r` into `m`.

We don't need to compute the full entropy over the old and new sets of bins,
because the entropy contributions of bins not related to the split or merge
cancel out. As such, we can calculate this with only six terms as follows,
where `p(bin) = bin.count / total`.

```
H(new) - H(old) =   [ -p(s_l) log p(s_l) - p(s_r) log p(s_r) - p(m) log p(m) ]
                  - [ -p(s) log p(s) - p(m_l) log p(m_l) - p(m_r) log p(m_r) ]
```

__Algorithm pseudocode.__ This leaves us with the following algorithm for
an update.

```
def update(score):
  if there aren't N bins yet:
    insert Bin(lower_divider=score, count=1)
    return

  s = score's bin
  s.count++

  m_l, m_r = smallest contiguous pair of bins separate from s
  m = Bin(lower_divider=m_l.lower_divider, count=m_l.count + m_r.count)

  s_l = Bin(lower_divider=s.lower_divider, count=s.count/2)
  s_r = Bin(lower_divider=score, count=s.count/2)

  if H(new) > H(old):
    replace s with s_l and s_r
    replace m_l and m_r with m
```

## Performance

We did some benchmarking under these conditions:
 - Two tests: samples from `Beta(2,5)`, and a power-law distribution.
 - The bin-entropy algorithm targeting 5 bins.
 - The 'simple' algorithm from above storing `N` samples.
 - The 'better-simple' algorithm that stores the last `N` samples and uses
   them to decide the dividers for 5 bins.

Results showed that the bin-entropy algorithm performs about as well as the
better-simple algorithm when `N = 150`, and actually slightly outperforms it for
`N < 150`. This may be due to the split/merge making it less sensitive to noise.
It's difficult to compare the results of the simple algorithm because the number
of bins is different, but the results were significantly noisier than the
better-simple algorithm.

# Ideas for improvement

Here are some things that anecdotally improve performance:

 - The algorithm loses information when a bin is split, because splitting the
   counts evenly is uninformed. Tracking moments of the distribution within
   each bin can help make this a more informed choice.

 - We only perform one split/merge operation per insert, but it's possible
   several split-merges should be done after one insert.

 - Allowing the merged bins to contain one of the split bins provides a small
   improvement in performance.

Finally, we could replace this altogether with something that builds on FAME or
the P^2 quantile estimation algorithm.
