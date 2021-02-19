# Run test locally

This file contains instruction summary to run test(s) under this folder locally.
See [testing page](../docs/testing.md) for details.

# To run run_py_tests.py

Suppose you would like to run test testCanSetCheckboxWithSpaceKey.
First, build chrome and chromedriver binary.
```
autoninja -C out/Default chromedriver_py_tests

```
Then, either run
```
vpython chrome/test/chromedriver/test/run_py_tests.py --chromedriver=out/Default/chromedriver --filter=__main__.ChromeDriverTest.testCanSetCheckboxWithSpaceKey
```
or the abbreviating the filter
```
vpython chrome/test/chromedriver/test/run_py_tests.py --chromedriver=out/Default/chromedriver --filter=\*testCanSetCheckboxWithSpaceKey
```
