# Web Packaging Component (`components/web_package`)

**Parent:** [WebApps Guidelines](/components/webapps/AGENTS.md)

Shared implementation for parsing, serializing, and verifying Web Bundles
(`.wbn`) and Signed Web Bundles (`.swbn`), used from the browser process by
Isolated Web Apps and from the network service.

## Canonical Docs

- [Component Overview](README.md)
- [Signed Web Bundles Guidelines](signed_web_bundles/AGENTS.md)
- [Signed Web Bundles README](signed_web_bundles/README.md)
- [Core IWA Component Guidelines](/components/webapps/isolated_web_apps/AGENTS.md)

## Security & Architectural Invariants

- **Rule of 2 & Memory-Safe Parsing:** Untrusted Web Bundle and Integrity Block
  CBOR bytes are decoded only by the memory-safe Rust crates (`rust/` and
  `signed_web_bundles/rust/`), which is what allows `WebBundleParser` to run in
  unsandboxed processes such as the browser process. Never decode untrusted
  bundle bytes in C++; C++ code must only consume what the Rust crates return.
- **Target Encapsulation:** Internal header/implementation split `source_set`s
  in `BUILD.gn` and the internal Rust crates (`rust/`,
  `signed_web_bundles/rust/`) are implementation details of this component.
  External callers must depend on `//components/web_package` (or
  `//components/web_package/mojom` and `//components/web_package/test_support`).

## Command Line Execution

- **Unit Tests:**
  `tools/autotest.py -C out/Default components/web_package/`
- **Header & Visibility Check:**
  `gn check out/Default "//components/web_package/*"`
