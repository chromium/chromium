The Encrypted Reporting Pipeline (ERP) provides a universal method for upload of
data for enterprise customers.

The code structure looks like this:
Chrome:
  - //components/reporting
    Code shared between Chrome and Chrome OS.
  - //chrome/browser/policy/messaging_layer
    Code that lives only in the browser, primary interfaces for reporting data
    such as ReportQueue and ReportQueueConfiguration.
Chrome OS:
  - //platform2/missived
    Daemon for encryption and storage of reports.

If you'd like to begin using ERP within Chrome please check the comment in
[//chrome/browser/policy/messaging_layer/public/report_client.h](https:://chromium.googlesource.com/chromium/src/+/master/chrome/browser/policy/messaging_layer/public/report_client.h#25).
