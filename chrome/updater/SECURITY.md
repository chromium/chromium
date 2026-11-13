# Security Guidance for //chrome/updater

`//chrome/updater` installs and updates software either per-user (running as
the logged-in user) or system-wide (running as `NT AUTHORITY\SYSTEM` on
Windows and `root` on macOS and Linux). For general Chromium security
guidance, see [Security for Agents][security-for-agents] and the
[Chrome Security FAQ][faq].

## Security Boundaries

* **Standard user to `SYSTEM` / `root`:** In a system-wide installation, an
  unprivileged local user (`Medium` integrity or lower on Windows, non-`root`
  UID on macOS and Linux) must not be able to execute arbitrary code or modify
  arbitrary files or registry keys as `SYSTEM` / `root`.
* **Sandbox to user or system:** Processes inside a Chromium sandbox (`Low`,
  `Untrusted`, or `AppContainer` integrity on Windows; sandboxed processes on
  macOS and Linux) must not be able to use a per-user or system-wide updater
  to escape the sandbox or run arbitrary installers.

## Non-Boundaries / Out of Scope

Issues that require any of the following preconditions or show only the
following effects are working as intended (or functional `Type=Bug` issues)
and are not security vulnerabilities:

* **Same-user processes in per-user installs:** A per-user updater runs as the
  user and trusts same-user, non-sandboxed (`Medium` integrity on Windows,
  same effective UID on macOS and Linux) callers by design.
* **Administrator or `System`-integrity accounts on Windows:** Transitions
  from an elevated administrator (`High` integrity) or from another process
  already running at `System` integrity (such as `LocalService`,
  `NetworkService`, or `NT SERVICE\*` Session 0 services) to `SYSTEM` are not
  a security boundary defended by the updater. Do not assume code execution in
  an administrator or `System`-integrity service context as an attacker
  starting point.
* **`root` on macOS and Linux:** `root` to `root` is not a security boundary.
* **Crashes or handle closure without privilege escalation:** Local IPC inputs
  that only trigger a controlled crash (such as a `CHECK` failure) or close
  handles without demonstrating privilege escalation or memory corruption are
  functional bugs (`Type=Bug`), not security vulnerabilities.

[security-for-agents]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/security-for-agents.md
[faq]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md
