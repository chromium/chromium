# Variations End-to-End tests

The files in this directory contain tests that check if the variations seed
works with Chrome on different platforms and channels.

# Running tests locally

## TL;DR

Running locally has some limitations:

-   You can only run a subset of tests locally (`test_basic_rendering` won't
    run).

-   You need a machine with a specific operating system to test different
    platforms.

If you want to bypass those limitations you need to run tests on the build bots
instead (see section `Other ways of triggering the tests`).

To run the tests on native Linux/Windows/MacOS platform on the latest `dev`
Chrome release you need to navigate to the Chrome source directory (works also
with Cider G) and invoke:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations`

## Tests invocation

The variation smoke test uses pytest framework to write and run tests. It
requires the source root to be in PYTHONPATH. This is done in two ways:

1.  invoke vpython3 from the source root;

2.  passing `-c chrome/test/variations/pytest.ini` to pytest.

It is always a good idea to invoke tests using `vpython3` as this set ups venv
the same way as in a test bot:

`chromium/src $ vpython3 -m pytest chrome/test/variations`

You can also compile the target:

`chrome/test/variations:variations_desktop_smoke_tests`

This will produce a wrapper script under:

`out/dir/bin/run_variations_desktop_smoke_tests`

This is equivalent to:

`cd out/dir && vpython3 -m pytest chrome/test/variations -c
../../chrome/test/variations/pytest.ini`

To get a list of supported parameters:

`chromium/src $ vpython3 -m pytest chrome/test/variations -h`

Test `test_basic_rendering` requires the host to be allowlisted in the Skia Gold
service. In general, only the test bots should be on that list. Therefore local
runs of this test will most likely fail. To ignore this test please pass
following flag to pytest:

`chromium/src $ vpython3 -m -k 'not test_basic_rendering' pytest
chrome/test/variations`

## What version of Chrome am I actually testing?

By default you're always testing latest `dev` Chrome release. If you want to
test different version you can pass:

-   argument `--channel` to choose the newest version for a specific channel
    (stable, beta, dev and canary) or

-   argument `--chrome-version` to choose specific Chrome version.

-   argument `--chromedriver` to pass a path to a custom Chromedriver directory.
    In this directory you can use any Chrome binary you want (e.g. compiled with
    your local changes).

## What version of the seed am I actually testing?

By default you're testing the seed that is located in file
`chrome/test/data/variations/variations_seed.json`. If you want to test another
seed version you can pass:

-   argument `--seed-file` with path to custom seed to test.

## Running on different platforms

The same tests can run on almost all the platforms: (win, linux, mac, cros,
android and WebView). However not all platforms can run on the same target
platform.

### Android

Required operating system: Linux.

Your environment has to be configured for Android builds. This is not currently
supported by Cider G. In your gclient configuration please set:
`target_os=["android"]` and run `gclient sync`.

Then you should be able to invoke the tests:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations
--target-platform=android
--avd-config=tools/android/avd/proto/android_30_google_apis_x86.textpb`

### Android WebView

Required operating system: Linux.

Your environment has to be configured for Android builds. This is not currently
supported by Cider G. In your gclient configuration please set
`target_os=["android"]` and run `gclient sync`.

Then you should be able to invoke the tests:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations
--target-platform=webview
--avd-config=tools/android/avd/proto/android_30_google_apis_x86.textpb`

### ChromeOS

Required operating system: Linux.

Your environment has to be configured for ChromeOS builds. This is not currently
supported by Cider G. In your gclient configuration please update your custom vars to include

```
    "custom_vars" : {
      "download_remoteexec_cfg": True,
      "cros_boards": "betty",
    }
```

and also set `target_os=["chromeos"]` then run `gclient sync`.

You need to download ChromeOS VM with command:

`cros chrome-sdk --board=betty --download-vm`

If you already have the VM, but want to download a fresh copy (e.g. because your
VM image got corrupted) you can re-download it using the flag:

`cros chrome-sdk --board=betty --download-vm --clear-sdk-cache`

You should be able to invoke the tests:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations
--target-platform=cros`

If the tests appear stuck, you can open the VM window with VNC Viewer to inspect
the current system state: `vncviewer localhost:5900`

### Linux

Required operating system: Linux.

Since we're running tests on native platform, you can run them with simple
version of the command:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations`

### Mac

Required operating system: Mac OS.

Since we're running tests on native platform, you can run them with simple
version of the command:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations`

### Windows

Required operating system: Windows.

Since we're running tests on native platform, you can run them with simple
version of the command:

`vpython3 -m pytest -k 'not test_basic_rendering' chrome/test/variations`

# Other ways of triggering the tests

There are generally two other ways to trigger tests:

## CI/CQ runs

If you want to test you changes on the real build bot, you can select custom
Tryjob in the Gerrit UI. Depending on which platform you want to check, please
choose one of the following builders:

-   android-emulator-finch-smoke-chrome (note: Android WebView tests use the
    same builder).
-   chromeos-betty-finch-smoke-chrome
-   linux-finch-smoke-chrome
-   mac-arm64-finch-smoke-chrome
-   win-finch-smoke-chrome

### What Chrome version am I actually testing?

You're testing the latest `dev` Chrome release.

### What version of seed am I actually testing?

By default you're testing the seed that is located in file
`chrome/test/data/variations/variations_seed.json`.

The test can switch to the latest daily built seeds in this repo:
https://chrome-infra-packages.appspot.com/p/chromium/chrome/test/data/variations/cipd

Which is brought into //components/variations/test_data/cipd via DEPS.

## Server seed changes (presubmit tests)

The seed changes will trigger a swarming test to execute tests using the seed
with seed changes; this setup is to ensure any seed changes will not break
Chrome itself.

# Chrome under test

The test doesn't attempt to compile Chrome but rather fetches from GCS, or can
be fed from the command line. So the tests here don't need to be branched off or
run on a branched builder.

The tests communicates with Chrome using webdriver.
