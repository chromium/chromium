# Run test(s) locally

This file contains instruction summary to run test(s) under this folder locally.
See the [testing page](../docs/testing.md) for details.

# To run `run_py_tests.py`

Suppose you would like to run the `testCanSetCheckboxWithSpaceKey` test.
First, build the `chrome` and `chromedriver` binaries.

```
autoninja -C out/Default chromedriver_py_tests
```

Then, either run:

```
chrome/test/chromedriver/test/run_py_tests.py --chromedriver=out/Default/chromedriver --filter=__main__.ChromeDriverTest.testCanSetCheckboxWithSpaceKey
```

or, by abbreviating the filter:

```
chrome/test/chromedriver/test/run_py_tests.py --chromedriver=out/Default/chromedriver --filter=\*testCanSetCheckboxWithSpaceKey
```
