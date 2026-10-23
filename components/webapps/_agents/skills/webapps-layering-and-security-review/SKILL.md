---
name: webapps-layering-and-security-review
description: >-
  Reviews changes in components/webapps/, chrome/browser/web_applications/, and
  chrome/browser/ui/web_applications/ for browser-process Mojo origin validation
  via RenderFrameHost and bad-message reporting. Use when reviewing Web Apps
  Mojo services or origin checks.
---

### `WEBAPPS_DERIVE_MOJO_ORIGIN_FROM_RENDER_FRAME_HOST`: Derive caller origin from `RenderFrameHost` in Mojo services and report bad messages

- **Severity**: ERROR
- **Why**: Renderer processes are untrusted. A compromised renderer can forge
  arbitrary URLs or origins in Mojo message parameters to bypass same-origin or
  permission checks unless validated against browser-side `RenderFrameHost`
  state.
- **Look for**:
  - Browser-process Mojo service implementations (`content::DocumentService`
    subclasses such as `WebInstallServiceImpl` and `SubAppsServiceImpl`) in
    `chrome/browser/web_applications/`, `chrome/browser/ui/web_applications/`,
    or `components/webapps/` that trust a renderer-supplied `url::Origin` or
    `GURL` for caller identity or same-origin checks instead of using `origin()`
    or `render_frame_host().GetLastCommittedOrigin()`.
  - Missing `mojo::ReportBadMessage()` / `ReportBadMessageAndDeleteThis()` when
    a renderer sends malformed arguments or invokes a Mojo method from a
    disallowed frame or origin context that Blink should have prevented.
- **Do not flag**:
  - Resolving a relative URL against `origin().GetURL()` followed by
    `origin().IsSameOriginWith(...)` and `ReportBadMessageAndDeleteThis(...)`.
  - Same-origin path crossing: the origin is the security boundary; `scope` only
    controls display and navigation routing, including W3C prefix matching
    (`"scope": "/app"` matches `/app-admin`).
  - Validated `scope_extensions` origins (which served
    `/.well-known/web-app-origin-association` over HTTPS naming the app)
    legitimately acting in-scope.
  - External (policy or preinstalled) installs loading `install_url` with
    `WebAppUrlLoader::UrlComparison::kSameOrigin` (cross-origin redirects are
    cancelled by the loader, so that path does not need an extra origin check in
    the navigation throttle).
  - Issues requiring Isolated Web App Developer Mode (`kIsolatedWebAppDevMode`,
    which is off by default).
