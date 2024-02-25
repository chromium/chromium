## About

This Chrome OS string matching library provides functionality to compute the
similarity scores between two given strings.

This library's main use is within the launcher backend ranking system
(`chrome/browser/ash/app_list/search/`).

The entry points to this library are via either:

1. `FuzzyTokenizedStringMatch`
    * For fuzzy matching.
1. `TokenizedStringMatch`
    * For non-fuzzy matching.

## Testing

The `FuzzyTokenizedStringMatch` class is complex and evolving. As such, we do
not currently enforce many specific and strict scoring requirements for sample
query-text pairs. However, there are tests which optionally log detailed
scoring output for manual inspection.

If making changes to fuzzy string matching functionality, please inspect the
benchmarking unit tests with verbose logging enabled. Steps:

Add to gn args:

```
# For building tests.
target_os = "chromeos"
```

Build and run tests, e.g.:

```sh
autoninja -C out/Default chromeos_unittests && out/Default/chromeos_unittests --gtest_filter="*FuzzyTokenizedStringMatchTest.Benchmark*" --v=1
```
