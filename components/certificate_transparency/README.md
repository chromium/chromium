# Certificate Transparency (CT)

This directory contains the implementation of the CT log list and Chrome's CT
policy enforcement.

The policy enforcement implementation here builds on the interfaces defined in
`net::TransportSecurityState::RequireCTDelegate` and `net::CTPolicyEnforcer` to
implement Chrome's CT policy. See `ChromeRequireCTDelegate` and
`ChromeCTPolicyEnforcer` for implementation details, respectively.

The log list format is defined in `certificate_transparency.proto`, and Chrome’s
CT configuration format is defined in `certificate_transparency_config.proto`.
A built-in log list is included in `data/log_list.json`; updates to the log list
are delivered via component updater (via the
[“PKI Metadata” component](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/component_updater/pki_metadata_component_installer.h;drc=288023bfb5c392e8b2643f2bd68ab86a67c1789a))
and take precedence over this built-in log list when available. The built-in
copy of the log list is updated via automated commits. The built-in log list is
compiled into a C++ source file by the script in
`tools/make_ct_known_logs_list.py`, via the action defined in `data/BUILD.gn`.

This component also contains various pref and feature definitions, and the C++
APIs used to interact with the log list.

For more information about Certificate Transparency, see
https://certificate.transparency.dev/. For more information about Chrome's
Certificate Transparency policies, see https://goo.gl/chrome/ct-policy and
https://goo.gl/chrome/ct-log-policy.
