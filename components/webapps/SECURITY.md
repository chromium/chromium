# Web Apps Shared Components Security Notes

Supplemental security facts for `//components/webapps/*` (excluding test-only
code), supplementing Chromium's
[Security for Agents](/docs/security/security-for-agents.md). Before filing a
bug, review Security for Agents to double-check your findings.

## Threat Model

- Process & Sandbox: Runs in the unsandboxed browser process, except
  `renderer/`, which runs in the sandboxed Renderer to extract page metadata.
- Compromised Renderer: Defended against by browser-process code.
  `ManifestManagerHost` (`//content/browser/manifest/`) terminates the Renderer
  via `mojo::ReportBadMessage()` if a parsed manifest's `start_url`, `id`, or
  `scope` is not same-origin with the document.
- Mojo IPC: Caller origins come from the browser-side `RenderFrameHost`, never
  message fields.
- Rule of Two: Manifests are parsed and icons decoded in a Renderer; Signed Web
  Bundles are parsed in a sandboxed utility process; origin association JSON is
  parsed with the memory-safe `base::JSONReader`.

## Explicit Non-bugs

- Cross-origin manifest files: Loading a manifest cross-origin (such as a CDN)
  with valid CORS headers while `start_url`, `id`, and `scope` remain
  same-origin with the document.
- IWA dev-proxy loading: Issues in `IwaSourceProxy` loading (`HandleProxy()`) or
  the `ws://` CSP allowance for HTTP dev proxies. Proxy sources are always dev
  mode, and loading them requires IWA Developer Mode, which is off by default;
  not a bug unless an attacker can enable that mode.
