# `UrlPatternIndex` overview

The UrlPatternIndex component can be used to build an index over a set of URL
rules, and speed up matching network requests against these rules.

A URL rule (see `flat::UrlRule` structure) describes a subset of network
requests that it targets. The essential element of the rule is its URL pattern,
which is a simplified regular expression (a string with wildcards).
`UrlPatternIndex` is mainly based on text fragments extracted from the patterns.

The component uses the [FlatBuffers serialization
library](https://google.github.io/flatbuffers/) to represent the rules and the
index. The key advantage of the format is that it does not require
deserialization. Once built, the data structure can be stored on disk or
transferred, then copied/loaded/memory-mapped and used directly.

# Detailed design

## `UrlPattern`s

The component is built around an underlying concept of a URL pattern, defined in
the class `UrlPattern`. These patterns are largely inspired by patterns in
[EasyList / Adblock Plus filters](https://adblockplus.org/filter-cheatsheet) and
are documented in more detail in the [declarativeNetRequest
documentation](https://developer.chrome.com/docs/extensions/reference/declarativeNetRequest/#type-RuleCondition).

## Building the index

The underlying goal of the index format is to efficiently check to see if URLs
match any URL patterns contained in the index. The data structure used here is
an N-gram filter. An N-gram is a string consisting of N (up to 8) bytes. Currently,
the component has chosen to use [`kNGramSize = 5`](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/url_pattern_index.h;drc=e89a43f45befc5c8e549d765018524d2f81c8765;l=54).

The strategy used in this component is to build a data structure which maps
`NGram -> vector<UrlRule>`, by finding all N-grams associated with a given URL
pattern, and picking one of them (the most distinctive one, see
`UrlPatternIndexBuilder::GetMostDistinctiveNGram`). The URL pattern is then
inserted into the map associated with that N-gram.

Note: URL patterns have special characters like `*` and `^` which implement
special wildcard matching. N-grams are built only _between_ these special
characters.

For example, the URL pattern `foo.com/*abc*` will generate the following 5-grams:
```
foo.c
oo.co
o.com
.com/
```

See
[url_pattern_index.fbs](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/flat/url_pattern_index.fbs)
for the raw underlying Flatbuffers format which builds the N-gram filter using a
[custom hash table](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/closed_hash_map.h)
implementation.

## Querying the index

Querying a built index is very similar to building the index in the first place.
Given a URL, it is broken into all of it's component N-grams, just like the URL
pattern was above. For example, the URL `https://foo.com/?q=abcdef` would
generate the following 5-grams:
```
https
ttps:
tps:/
ps://
s://f
://fo
//foo
/foo.
foo.c
oo.co
o.com
.com/
com/?
om/?q
m/?q=
/?q=a
?q=ab
q=abc
=abcd
abcde
bcdef
```
With these N-grams extracted, we can just consider all of the `UrlPattern`s
which are associated with those N-grams. See `FindMatchInFlatUrlPatternIndex`
and `FindMatchAmongCandidates` for this logic.

Many of these N-grams match ones that are also present in the `foo.com/*abc*`
example above , so we can be sure that that URL pattern will be considered
during pattern evaluation.

## Fallback rules
You might be thinking "what about URLs whose length is less than N, or
patterns that generate no N-grams?" We will make sure to put all rules like that
into a special list called the `fallback_rules` which are applied to every URL
unconditionally.

## Checking an individual `UrlPattern`

This logic is encapsulated in `UrlPattern::MatchesUrl`. This essentially
consists of splitting a URL pattern by the `*` wildcard, and considering each
subpattern in between the `*`s.

There is some complexity here to deal with:
- `^` separator matching, which matches any ASCII symbol except letters, digits,
  and the following: `'_', '-', '.', '%'`. See
  [fuzzy_pattern_matching](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/fuzzy_pattern_matching.h).
- `|` Left/right anchors, which specifies the beginning or end of a URL.
- `||` Domain anchors, which specifies the start of a (sub-)domain of a URL.

After all this complexity is dealt with, the bulk of the subpattern logic is
simply `StringPiece::find / std::search`! This component used to use something
much more complicated ([Knuth-Morris-Pratt
algorithm](https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm)),
but benchmarking on real URLs proved the simple solution was more optimal (and
removed the need for a preprocessing step at indexing time), so it was
[removed](https://codereview.chromium.org/2793993002/).

For example, in checking if `https://foo.com/?q=abcdef` matches `foo.com/*abc*`,
the component will:

- Split the URL pattern into two pieces: `foo.com/` and `abc`.
- Try to find `foo.com/` in `https://foo.com/?q=abcdef`, which is a match!
- Remove the matching prefix
- Try to find `abc` in `?q=abcdef`, which is a match! This is the last pattern,
  so return true