# Cross platform updater integration tests

This folder contains the integration tests for testing cross-platform updater
on the supported platforms.

The tests are written using `unittest` framework from python and use [`typ`](https://pypi.org/project/typ/)
library to execute them. [run_updater_tests.py](../run_updater_tests.py) is the
script that drives test runs on test machines using typ library.

To write a new integration test, extend it from `unittest.TestCase` class and
declare test function as `test_xyz()` so that typ can easily discover the tests
and run them.

Currently the folder contains [a sample test](hello_test.py) written to test end-to-end
integration of the python test framework with chromium test infrastructure. This
is a work in progress and more code as per cross-platform updater test framework
design doc will be added here.

# Platforms Supported
* win: win7, win10
* mac: macOS 10.10-11.0 (11.0 arm64 support to be added soon)

# Directory Structure
* common/ contains all utility functions shared across the codebase.

* mock_server/ contains code to configure local HTTP/HTTPS server that mocks
  behavior of real omaha server. The client sends request to this mock server
  and receives response based on pre-configured rules and predicates.

* updater/ contains modules to support updater testing.
