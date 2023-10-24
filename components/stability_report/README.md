This component registers a `crashpad::UserStreamDataSource` with the `crash`
component. Whenever a process crashes, it collects stats on the resources used
by the crashed process (e.g. memory in use and available, open file handles)
and writes them to the stream as a serialized `StabilityReport` proto.

This is currently only implemented on Windows, Linux, and ChromeOS.

Google-internal details: The `StabilityReport` is parsed by the crash server
and displayed in the Breadcrumbs tab of the crash UI. The parsing code is at
http://shortn/_Xt58wqlKTk.
