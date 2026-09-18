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

- **Shared Components** (`components/webapps/`): [README](README.md)
  Cross-platform installability, promotion, and identifiers shared by Desktop
  and Android (delegates via `WebappsClient`). `browser/installable/` ·
  `browser/banners/` · `browser/android/` · `common/` · `renderer/` ·
  `isolated_web_apps/`
- **Desktop Engine** (`chrome/browser/web_applications/`):
  [Rules](/chrome/browser/web_applications/AGENTS.md) ·
  [README](/chrome/browser/web_applications/README.md) Per-profile
  `WebAppProvider` engine (commands, locks, DB storage, sync, and OS
  integration).
- **Desktop UI** (`chrome/browser/ui/web_applications/`):
  [Rules](/chrome/browser/ui/web_applications/AGENTS.md) ·
  [README](/chrome/browser/ui/web_applications/README.md) Desktop UI
  controllers, window management, omnibox actions, navigation capturing, and Mac
  App Shims.
- **Desktop Views UI** (`chrome/browser/ui/views/web_apps/`):
  [Rules](/chrome/browser/ui/views/web_apps/AGENTS.md) ·
  [README](/chrome/browser/ui/views/web_apps/README.md) Views dialogs, bubbles,
  custom frame toolbars, and integration test driver.
- **Android Java Webapps**
  (`chrome/android/java/src/org/chromium/chrome/browser/webapps/`):
  [Rules](/chrome/android/java/src/org/chromium/chrome/browser/webapps/AGENTS.md)
  ·
  [README](/chrome/android/java/src/org/chromium/chrome/browser/webapps/README.md)
  · [Architecture](docs/android_architecture.md) Java activity lifecycles and
  coordinators.
- **Android Native WebAPK** (`chrome/browser/android/webapk/`):
  [Rules](/chrome/browser/android/webapk/AGENTS.md) ·
  [README](/chrome/browser/android/webapk/README.md) Browser-process native
  WebAPK services and sync.
- **Manifest Parsing** (`third_party/blink/renderer/modules/manifest/`):
  [Rules](/third_party/blink/renderer/modules/manifest/AGENTS.md) ·
  [Common](/third_party/blink/public/common/manifest/AGENTS.md) ·
  [README](/third_party/blink/renderer/modules/manifest/README.md) JSON manifest
  parsing in untrusted renderer.
- **Manifest Fetching** (`content/browser/manifest/`):
  [Rules](/content/browser/manifest/AGENTS.md) ·
  [README](/content/browser/manifest/README.md) Browser-side manifest fetching
  and icon downloading coordination.

## Verification & Testing

- Prefer `tools/autotest.py` for test compilation and execution.
- Before running Android instrumentation tests (`chrome_public_test_apk`), check
  `adb devices`. If no online device or emulator is active, pause and ask the
  user to start an emulator.
- For copy-pasteable test targets, commands, and runner recipes across Desktop,
  Android, and Shared Components, load skill `webapps-dev`.
