# Domain Reliability

This directory contains the implementation of Domain Reliability, which provides
out-of-band reporting of network errors encountered when connecting to certain
Google properties, enabling client-side reliability monitoring for those
domains.

Domain Reliability is the precursor to
[Network Error Logging](https://w3c.github.io/network-error-logging/) and
[Reporting](https://w3c.github.io/reporting/),  which are general solutions
available for any origin (not just Google-owned domains).

The implementation is structured similarly to a combination of Network Error
Logging (see `//net/network_error_logging/`) and Reporting (see
`//net/reporting`).

Googlers may refer to
[go/client-side-reliability-monitoring](http://go/client-side-reliability-monitoring).
