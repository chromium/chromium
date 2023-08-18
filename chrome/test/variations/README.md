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

# Running tests

The variation smoke test uses pytest framework to write and run tests. It
requires the source root to be in PYTHONPATH. This is done in two ways:

1) invoke vpython3 from the source root;
2) passing `-c chrome/test/variations/pytest.ini` to pytest.

It is always a good idea to invoke test using `vpython3` as this set up venv
the same way as in a test bot:

`chromium/src $ vpython3 -m pytest chrome/test/variations`

You can also compile the target:
`chrome/test/variations:variations_desktop_smoke_tests`

This will produce a wrapper script under:
`out/dir/bin/run_variations_desktop_smoke_tests`

This is equivalent to:
`cd out/dir && vpython3 -m pytest chrome/test/variations -c ../../chrome/test/variations/pytest.ini`

To get a list of supported parameters:
`chromium/src $ vpython3 -m pytest chrome/test/variations -h`

## Running on different channels or platforms

Same tests can run on almost all the platforms: (win, linux, mac, cros,
android and webview). However not all the platforms can run on the same
target platform. For example, (cros, android and webview) is only supported
on Linux. Windows and Mac can only run on their own OS.

The argument `--channel` can be set to (stable, beta, dev and canary).

The argument `--chrome-version` can be set to a particular version.
