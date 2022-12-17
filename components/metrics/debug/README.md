# Metrics Internals Debug Page

**chrome://metrics-internals** is a debug page that reports the state of various
metrics systems.

The page displays logs that have been collected by the UMA metrics service,
which are eventually sent to Google servers. These logs can be exported, and
their proto data can later be inspected using a Google-internal tool:
[go/metrics-internals-inspector](http://go/metrics-internals-inspector).

On debug builds, the page will show all logs collected since browser startup,
including logs from previous sessions that were not sent yet. On release builds,
the page will instead show logs collected since the page was opened. This
difference is mostly due to memory concerns (on official releases, we don't want
logs to be lingering in memory since they can be relatively large).

> Tip: By using the `--export-uma-logs-to-file=FILE_PATH` command line flag, all
logs collected throughout the Chrome session will be exported to the passed
`FILE_PATH` on shutdown (the file is created if it does not already exist). For
release builds, this flag also has the effect of showing all logs collected
since browser startup on the page.

> Note: The delay between logs being closed can be [long]. If you are just
testing to see if a certain metric (e.g., histogram, user action, etc.) is being
properly sent to Google servers and you want logs to be collected more often,
you can use the `--metrics-upload-interval=N` command line flag, where `N` is in
seconds and is at least `20`.

[long]: https://source.chromium.org/chromium/chromium/src/+/main:components/metrics/net/cellular_logic_helper.cc;l=18,21;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd

Unlike other metrics debug pages ([chrome://user-actions], **chrome://ukm**, and
[chrome://histograms]), this page focuses mainly on what has actually been sent
to Google servers. For example, it is possible that a user action shown on
[chrome://user-actions] is never actually sent (e.g., due to being [truncated]
for bandwidth reasons). Similarly, a source shown on **chrome://ukm** might be
[dropped] and hence never be sent. Or, a histogram shown on
[chrome://histograms] would not have been collected under regular circumstances
(e.g., subprocess histograms, which are normally only collected periodically,
but are collected [immediately] when loading **chrome://histograms**). In other
words, this page displays the actual logs that have been created and sent,
without modifications.

[chrome://user-actions]: https://chromium.googlesource.com/chromium/src/+/master/tools/metrics/actions/README.md#Testing
[chrome://histograms]: https://chromium.googlesource.com/chromium/src/+/master/tools/metrics/histograms/README.md#Testing
[truncated]: https://source.chromium.org/chromium/chromium/src/+/main:components/metrics/metrics_log.cc;l=552;drc=38321ee39cd73ac2d9d4400c56b90613dee5fe29
[dropped]: https://source.chromium.org/chromium/chromium/src/+/main:components/ukm/ukm_recorder_impl.cc;l=362;drc=38321ee39cd73ac2d9d4400c56b90613dee5fe29
[immediately]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/metrics/histograms_internals_ui.cc;l=100;drc=5e521f43547ebdce502a555c5edb3a18f0c87a8a
