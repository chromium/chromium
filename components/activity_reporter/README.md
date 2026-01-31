`activity_reporter` is responsible for reporting when the browser (or other
embedder) is actively used to the browser's vendor. This enables the vendor to
count the number of active users of the browser.

Historically, this data was reported only through the updater: the browser would
signal the updater that it had been used, and when the updater next checked for
updates, it would tell the update server whether or not the browser had been
actively used. This report happens regardless of usage-stat opt-in, in a
de-identified and privacy-preserving way.

`activity_reporter` re-implements that same function, but separate from the
updater, to better isolate functionality. It may be disabled by the
`report_activity=false` GN arg.

The [update protocol documentation](//docs/updater/protocol_4.md) describes the
transmitted information and deduplication algorithm in more detail.
