The Encrypted Reporting Pipeline (ERP) provides a universal method for upload of
data for enterprise customers.

The code structure looks like this:

Chrome:
  - `//components/reporting` \
    Code shared between Chrome and ChromeOS.
  - `//chrome/browser/policy/messaging_layer` \
    Code that lives only in the browser, primary interfaces for reporting data
    such as `ReportQueueImpl` and `ReportQueueConfiguration`.

ChromeOS:
  - `//platform2/missived` \
    Daemon for encryption and storage of reports.

If you'd like to begin using ERP within Chrome please check the comment in
[//components/reporting/client/report_queue_provider.h](https://chromium.googlesource.com/chromium/src/+/main/components/reporting/client/report_queue_provider.h#25).

### Run Unit Tests

To run the unit tests for this directory, after having configured Chromium's
build environment:

1. Run `autoninja -C out/Default components_unittests` to build the components
   unit test executable.

1. Then, run `out/Default/components_unittests --gtest_filter='<target tests>'`
   to run relevant tests. Here, `<target tests>` is a wildcard pattern (refer to
   the document of gtest for more details). For example, to run all tests for
   `StorageQueue`, run

       $ out/Default/components_unittests --gtest_filter='*/StorageQueueTest.*'

   For another example, to run all tests in this directory, run

       $ tools/autotest.py -C out/Default --run_all components/reporting

   You can also append a filter such as `--gtest_filter='*/StorageQueueTest.*'`
   to the line above.

   Another useful flag for dealing with flaky tests is `--gtest_repeat=`, which
   repeats tests for multiple times.

   For more gtest features, check out
   [the gtest document](https://google.github.io/googletest/advanced.html).
