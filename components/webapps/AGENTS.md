# Web Applications Central Hub (components/webapps)

Central Hub for Progressive Web Apps (PWAs), WebAPKs, Isolated Web Apps (IWAs),
and Web App Manifest processing in Chromium.

## Gotchas & Invariants

- **App Service Decoupling:** Do NOT add new dependencies on
  `//components/services/app_service` or `apps::` types. Always define or use
  `webapps::` types.
- **Downward Layering:** Architecture flows strictly downward: Embedders
  (`chrome/browser/web_applications/`, `chrome/android/`) depend on Shared
  Components (`components/webapps/`), which depend on `content/` and
  `third_party/blink/`. Lower layers never depend on higher layers. Enforced by
  DEPS.

## Ecosystem Map (Directories & Spokes)

- **Shared Components** (`components/webapps/`): [README.md](README.md) ·
  [SECURITY.md](SECURITY.md) Cross-platform installability, promotion, and
  identifiers shared by Desktop and Android (delegates via `WebappsClient`).
  `browser/installable/` · `browser/banners/` · `browser/android/` · `common/` ·
  `renderer/`
- **Isolated Web Apps Component** (`components/webapps/isolated_web_apps/`):
  [AGENTS.md](isolated_web_apps/AGENTS.md) ·
  [README.md](isolated_web_apps/README.md) Core browser-agnostic IWA runtime,
  bundle operations, reader registry, key distribution, `isolated-app://` URL
  loading, and cryptographic identity verification.
- **Web Packaging & Signed Web Bundles** (`components/web_package/`):
  [AGENTS.md](/components/web_package/AGENTS.md) ·
  [`signed_web_bundles/AGENTS.md`](/components/web_package/signed_web_bundles/AGENTS.md)
  · [README.md](/components/web_package/README.md) Memory-safe Rust/C++ Web
  Bundle (`.wbn`) and Signed Web Bundle (`.swbn`) parsing, Integrity Block v2,
  and signature verification.
- **Desktop Engine** (`chrome/browser/web_applications/`):
  [AGENTS.md](/chrome/browser/web_applications/AGENTS.md) ·
  [README.md](/chrome/browser/web_applications/README.md) ·
  [SECURITY.md](/chrome/browser/web_applications/SECURITY.md) Per-profile
  `WebAppProvider` engine (commands, locks, DB storage, sync, and OS
  integration).
- **Desktop UI** (`chrome/browser/ui/web_applications/`):
  [AGENTS.md](/chrome/browser/ui/web_applications/AGENTS.md) ·
  [README.md](/chrome/browser/ui/web_applications/README.md) Desktop UI
  controllers, window management, omnibox actions, navigation capturing, and Mac
  App Shims.
- **Desktop Views UI** (`chrome/browser/ui/views/web_apps/`):
  [AGENTS.md](/chrome/browser/ui/views/web_apps/AGENTS.md) ·
  [README.md](/chrome/browser/ui/views/web_apps/README.md) Views dialogs,
  bubbles, custom frame toolbars, and integration test driver.
- **Android Java Webapps**
  (`chrome/android/java/src/org/chromium/chrome/browser/webapps/`):
  [AGENTS.md](/chrome/android/java/src/org/chromium/chrome/browser/webapps/AGENTS.md)
  ·
  [README.md](/chrome/android/java/src/org/chromium/chrome/browser/webapps/README.md)
  · [Architecture](docs/android_architecture.md) Java activity lifecycles and
  coordinators.
- **Android Native WebAPK** (`chrome/browser/android/webapk/`):
  [AGENTS.md](/chrome/browser/android/webapk/AGENTS.md) ·
  [README.md](/chrome/browser/android/webapk/README.md) Browser-process native
  WebAPK services, minting, and update dispatch.
- **Manifest Parsing** (`third_party/blink/renderer/modules/manifest/`):
  [AGENTS.md](/third_party/blink/renderer/modules/manifest/AGENTS.md) ·
  [`public/common/manifest/AGENTS.md`](/third_party/blink/public/common/manifest/AGENTS.md)
  · [README.md](/third_party/blink/renderer/modules/manifest/README.md) JSON
  manifest parsing in untrusted renderer and validating `blink::Manifest` Mojo
  typemap.
- **Manifest Fetching** (`content/browser/manifest/`):
  [AGENTS.md](/content/browser/manifest/AGENTS.md) ·
  [README.md](/content/browser/manifest/README.md) Browser-side manifest
  fetching, contextual validation, and icon downloading coordination.

## Verification & Testing

- Prefer `tools/autotest.py` for test compilation and execution.
- Before running Android instrumentation tests (`chrome_public_test_apk`), check
  `adb devices`. If no online device or emulator is active, pause and ask the
  user to start an emulator.
- For copy-pasteable test targets, commands, and runner recipes across Desktop,
  Android, and Shared Components, load skill `webapps-dev`.
