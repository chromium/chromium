The Encrypted Reporting Pipeline (ERP) provides a universal method for upload of
data for enterprise customers.

The code structure looks like this:
Chrome:
  - //components/reporting
    Code shared between Chrome and Chrome OS.
  - //chrome/browser/policy/messaging_layer
    Code that lives only in the browser, primary interfaces for reporting data
    such as ReportQueueImpl and ReportQueueConfiguration.
Chrome OS:
  - //platform2/missived
    Daemon for encryption and storage of reports.

If you'd like to begin using ERP within Chrome please check the comment in
[//components/reporting/client/report_queue_provider.h](https:://chromium.googlesource.com/chromium/src/+/master/components/reporting/client/report_queue_provider.h#25).

### Run Unit Tests

To run the unit tests for this directory, after having configured Chromium's
build environment:

1. Run `autoninja -C out/Default components_unittests` to build the components
   unit test executable.

1. Then, run `out/Default/components_unittests --gtest_filter='<target tests>'` to
   run relevant tests. Here, `<target tests>` is a wildcard pattern (refer to
   the document of gtest for more details). For example, to run all tests for
   `StorageQueue`, run

       $ out/Default/components_unittests --gtest_filter='*/StorageQueueTest.*'
