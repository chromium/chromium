# Variations End-to-End tests

The files in this directory contain tests that check if the variations seed
works with Chrome on different platforms and channels.

There are generally two ways to trigger tests here:

1) CI/CQ runs: this currently uses committed seeds under test/data/variations;
the test can switch to the latest daily built seeds once they are available
here in this repo: https://chromium.googlesource.com/chromium-variations

2) Server seed changes: the seed changes will trigger a swarming test to
execute tests using the seed with seed changes; this setup is to ensure any
seed changes will not break Chrome itself.

# Chrome under test

The test doesn't attempt to compile Chrome but rather fetches from GCS, or can
be fed from the command line. So the tests here don't need to be branched off
or run on a branched builder.

The tests communicates with Chrome using webdriver.

# Running locally

The tests can be invoked using `testing/scripts/run_isolated_script_test.py`.

# Working in Progress

This folder is to migrate from `testing/scripts/run_variations_smoke_tests.py`
and be the main home for variations smoke tests.
