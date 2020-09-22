# Cross platform updater integration tests

This folder contains the integration tests for testing cross-platform updater
on the supported platforms.

The tests are written using `unittest` framework from python and use [`typ`](https://pypi.org/project/typ/)
library to execute them. [run_updater_tests.py](../run_updater_tests.py) is the
script that drives test runs on test machines using typ library.

To write a new integration test, extend it from `unittest.TestCase` class and
declare test function as `test_xyz()` so that typ can easily discover the tests
and run them.

Currently the folder contains a sample test written to test end-to-end
integration of the python test framework with chromium test infrastructure. This
is a work in progress and more code as per cross-platform updater test framework
design doc will be added here.

# Platforms Supported
* win: win7, win10
* mac: macOS 10.10-10.15
