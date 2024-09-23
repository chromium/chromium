# Testing

There are several test suites for verifying ChromeDriver's correctness:

| Test Suite | Purpose | Frequency |
| ---------- | ------- | --------- |
| Unit tests | Quick tests for verifying individual objects or functions | CQ |
| Python integration tests | Verify important features work correctly with Chrome | CQ |
| Web platform tests | Verify W3C standard complaince | CI waterfall |
| JavaScript unit tests | Verify ChromeDriver's JavaScript functions | CQ |

## Unit tests (`chromedriver_unittests`)

Many C++ source files for ChromeDriver have corresponding unit test files,
with filenames ending in `_unittest.cc`. These tests should be very quick (each
taking no more than a few milliseconds) and be very stable.
We run them on the commit queue on all desktop platforms.

Here are the commands to build and run the unit tests:

```bash
autoninja -C out/Default chromedriver_unittests
out/Default/chromedriver_unittests
```

## Python integration tests

These tests are maintained by the ChromeDriver team,
and are intended to verify that ChromeDriver works correctly with Chrome and
chrome-headless-shell.
They are written in Python script, in
[`test/run_py_tests.py`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/test/run_py_tests.py).
We run these tests on the CQ (commit queue) on all desktop platforms,
and plan to run them on Android as well in the future.

In the examples below `CHROMEDRIVER_DIR` is `chrome/test/chromedriver` if you
are in a chromium checkout.

To run these tests, first build Chrome and ChromeDriver, and then
invoke `run_py_tests.py`:

```bash
autoninja -C out/Default chrome chromedriver
vpython3 <CHROMEDRIVER_DIR>/test/run_py_tests.py --chromedriver=out/Default/chromedriver
```

The `run_py_tests.py` script has a number of options.
Run it with `--help` for more information.
The only require option is `--chromedriver` to specify the location of
the ChromeDriver binary.

### chrome-headless-shell

To run the tests you need to build chrome-headless-shell and ChromeDriver, and then
invoke `run_py_tests.py`:

```bash
autoninja -C out/Default headless_shell chromedriver
vpython3 <CHROMEDRIVER_DIR>/test/run_py_tests.py\
         --browser-name=chrome-headless-shell\
         --chromedriver=out/Default/chromedriver
```

### Test filtering

The `--filter` option can be used on `run_py_tests.py` command line to filter
the tests to run. Inside the filter, tests must be specified using the format
`moduleName.className.testMethodName`, where `moduleName` is always `__main__`,
`className` is the name of the Python class containing the test,
and `testMethodName` is the Python method defining the test.
For example `--filter=__main__.ChromeDriverTest.testLoadUrl`.

The `*` character can be used inside the filter as a wildcard.

To specify multiple tests in the filter, separate them with `:` characters.

### Disabling an integration test

If there are any test cases that fail or are flaky, and you can't fix them
quickly, please add the test names to one of the filters near the beginning
of `run_py_tests.py`. If the failure is due to a bug, please file an issue at
<https://crbug.com/chromedriver/new> and include the link to the issue as a
comment. If the failure is intentional (e.g., a feature is not supported on a
particilar platform), explain it in a comment.

### Running in Commit Queue

The Python integration tests are run in the Commit Queue (CQ) in steps
named `chromedriver_py_tests` and `chromedriver_py_tests_headless_shell`.

When running inside the CQ, the `--test-type=integration` option is passed to
the `run_py_tests.py` command line. This has the following effects:

* All tests listed in
  [`_INTEGRATION_NEGATIVE_FILTER`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/test/run_py_tests.py?q=_INTEGRATION_NEGATIVE_FILTER)
  are skipped. Tests in this list should have comments indicating why they
  should be skipped in the CQ.
* If there are a small number of test failures (no more than 10),
  then all failed tests are retried at the end.
  This is to prevent flaky tests from causing CQ failures.

### Testing on Android

The Python integration tests can be used to verify ChromeDriver interaction
with Chrome running on Android devices. This requires the following equipment:

* A Linux machine to run the Python script and ChromeDriver.
  (While ChromeDriver can also control Android Chrome from Windows and Mac, the
  Python integration tests only support controlling Android Chrome from Linux.)
* An Android device attached to the Linux machine. This device must have
  [USB debugging enabled](https://developer.android.com/studio/debug/dev-options#enable).

To run the tests, invoke `run_py_tests.py` with `--android-package=package_name`
option, where `package_name` can be one of the following values:

* `chrome_stable`: normal in-box Chrome that is installed by the system.
* `chrome_beta`: Beta build of Chrome.
* `chromium`: [Open source Chromium build](https://chromium.googlesource.com/chromium/src/+/main/docs/android_build_instructions.md).

There is future plan to [run these tests in the Chromium Commit
Queue](https://crbug.com/813466).

## Web Platform Tests (WPT)

The Web Platform Tests (WPT) project is a W3C-coordinated attempt to build a
cross-browser testsuit to verify how well browsers conform to web platform
standards. Here, we will only focus on the WebDriver portion of WPT.
You can either use the tests [bundled](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt)
with Chromium source code or the tests checked out externally from the
[WPT official repo](https://github.com/web-platform-tests/wpt).

### External WPT checkout

To run WPT WebDriver tests, first clone the tests from GitHub into an empty
directory:

```bash
git clone https://github.com/web-platform-tests/wpt
```

If necessary, install Python `virtualenv` module on your system.
This only needs to be done once. The command for the installation depends on
your system, but is usually something like:

```bash
pip install virtualenv
```

Now you can change into the WPT repository location,
and run WPT WebDriver tests with the following command:

```bash
./wpt run [options] chrome webdriver
```

Use `./wpt run --help` to see all available options.
The following are the most useful options:

* `--webdriver-binary /path/to/chromedriver` specifies the ChromeDriver binary
  to use. Without this option, the test runner will try to find ChromeDriver on
  your PATH, or download ChromeDriver if it is not already on the PATH.
* `--binary /path/to/chrome` specifies the Chrome binary to use.
* `--webdriver-arg=...` specifies additional arguments to be passed to the
  ChromeDriver command line. For example, to create a ChromeDriver verbose log,
  use `--webdriver-arg=--verbose --webdriver-arg=--log-path=/path/to/log
  --webdriver-arg=--append-log`. Note the following:
  * Each ChromeDriver switch needs a separate `--webdriver-arg`.
    Don't concatenate multiple switches together.
  * Each ChromeDriver switch must be connected with `--webdriver-arg` with
    an `=` sign.
  * `--webdriver-arg=--append-log` is recommended.
    Sometimes the test runner needs to restart ChromeDriver during tests,
    and this can cause ChromeDriver to overwrite logs without this switch.
* `--log-wptreport /path/to/report` generates a test report in JSON format.
  The test runner always displays test failures on the screen, but it is often
  useful to generate a test log as well. Many log formats are available,
  see WPT help for all options.

The WPT WebDriver tests are organized into subdirectories, one for each
WebDriver command defined in the W3C spec. You can select a subset of the tests
with the last argument on the WPT command line. For example, to run all tests
for the New Session command, use

```bash
./wpt run [options] chrome webdriver/tests/new_session
```

### Bundled WPT

The tests are located in [//third_party/blink/web_tests/external/wpt](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt)
directory.
You can use the following commands to run them:

```bash
autoninja -C out/Default chrome_wpt_tests
out/Default/bin/run_chrome_wpt_tests -t Default external/wpt/webdriver/path/to/test/or/dir
```

This will invoke [`//third_party/blink/tools/run_wpt_tests.py`][1], a thin
wrapper around wptrunner used to run WPTs (including webdriver tests) in Chromium CQ/CI.
See [these instructions] for suppressing failures for webdriver tests.
The [WPT importer] will automatically generate test expectations or baselines to suppress new
test failures.

[1]: /docs/testing/run_web_platform_tests.md
[these instructions]: /docs/testing/run_web_platform_tests.md#test-expectations-and-baselines
[WPT importer]: /docs/testing/web_platform_tests.md#Importing-tests

## JavaScript Unit Tests

All ChromeDriver JavaScript files in the
[js](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/js/)
directory have corresponding unit tests, stored in HTML files.
These tests can be run in two ways:

* They are run as part of the Python integration tests (`run_py_tests.py`)
  menteioned above, through a test class named `JavaScriptTests`.
  As a result of this, these tests are run in the CQ (commit queue).

* They can be run manually, by using Chrome to load the HTML file containing
  the tests. After the HTML file is loaded, open DevTools pane and switch to
  the Console tab to see the test results.
