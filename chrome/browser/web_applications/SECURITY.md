# Web Apps (WebAppProvider) Security Notes

Supplemental security facts for `//chrome/browser/web_applications/` (excluding
test-only code), supplementing
[components/webapps/SECURITY.md](/components/webapps/SECURITY.md) for general
webapps security, and Chromium's
[Security for Agents](/docs/security/security-for-agents.md). Before filing a
bug, review Security for Agents to double-check your findings.

## Threat Model

- Process & Sandbox: Runs in the unsandboxed browser process, except
  `chrome_pwa_launcher/`, a small unsandboxed Windows executable that launches
  Chrome.
- Compromised Renderer: Defended against. Manifests, icons, page metadata, and
  Mojo calls from Renderers are untrusted; origin checks use browser-side state
  such as the `RenderFrameHost`'s committed origin.
- Mojo IPC: `WebInstallServiceImpl` and `SubAppsServiceImpl` take the caller's
  origin from the `RenderFrameHost`, not message fields. They call
  `mojo::ReportBadMessage()` for disallowed contexts, and Sub Apps also does so
  for malformed install paths.
- Rule of Two: Blink parses manifests in a Renderer, memory-safe Rust parses
  Signed Web Bundles, and JSON (such as IWA update manifests) is parsed with the
  memory-safe `base::JSONReader`.

## Explicit Non-bugs

- Same-origin paths: The origin is the boundary; `scope` only controls display
  and navigation routing. Crossing paths on one origin is not a bug, including
  W3C prefix matching where `"scope": "/app"` matches `/app-admin`.
- Validated scope extensions acting in scope: An origin listed in
  `scope_extensions` that serves a valid
  `/.well-known/web-app-origin-association` over HTTPS naming the app
  (`WebAppOriginAssociationManager`) may badge, open in the app window without
  the origin bar, show notifications with the app's name and icon, use the app's
  display mode (including fullscreen), and capture navigations.
- Silent secondary identity updates: For user-installed apps, changing `name`,
  `short_name`, or the primary icon requires user approval. Theme and background
  colors, shortcuts, and primary icon changes below the threshold in
  `docs/manifest_update_process.md` update without a prompt.
- Shared background `WebContents` reuse: Claims that a compromised Renderer for
  one site stays attached when a later command loads a different site in the
  shared `WebContents`. Site Isolation gives the new site a new process.
- Developer-only modes: Issues requiring IWA Developer Mode
  (`kIsolatedWebAppDevMode`, off by default) or dev-proxy loaders, unless an
  attacker can enable that mode.

## Accepted Risks

### Shared background `WebContents` (`SharedWebContentsLock`)

`WebAppCommandManager` owns one hidden `WebContents` per profile. Only one
command holds it at a time, through `SharedWebContentsLock` or
`SharedWebContentsWithAppLock`. Users include sync, policy, and preinstalled
installs, Web Install, manifest updates, and icon repair. Precautions:

- When a command loads a cross-site URL, Site Isolation gives it a different
  Renderer process.
- After each command, a posted task destroys the `WebContents` and its Renderer
  unless another command already holds the lock
  (`WebAppCommandManager::ClearSharedWebContentsIfUnused`). Reuse only happens
  when commands run back-to-back, such as batched sync or policy installs.
- If a sync install's loaded manifest does not produce the expected app ID, the
  install falls back to the synced data.
- External installs (policy, preinstalled) load `install_url` with
  `WebAppUrlLoader::UrlComparison::kSameOrigin`. Cross-origin redirects are
  cancelled and never used as the install source.
