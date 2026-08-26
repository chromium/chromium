# Web Applications Central Hub (components/webapps)

Central Hub for Progressive Web Apps (PWAs), WebAPKs, Isolated Web Apps (IWAs),
and Web App Manifest processing in Chromium.

## Navigation & Canonical Docs

- [Code Structure](_agents/CODE_STRUCTURE.md) |
  [Dependencies](_agents/DEPENDENCIES.md) |
  [Harness Standards](_agents/_harness/AGENTS.md)
- [Component Overview](README.md) |
  [Desktop Provider](/chrome/browser/web_applications/README.md) |
  [Android Architecture](docs/android_architecture.md) |
  [Testing Guide](/chrome/browser/web_applications/docs/testing.md)

## Satellite Spokes

- [Desktop Backend](/chrome/browser/web_applications/AGENTS.md)
- [Desktop UI](/chrome/browser/ui/web_applications/AGENTS.md)
- [Desktop Views UI](/chrome/browser/ui/views/web_apps/AGENTS.md)
- [Android Native WebAPK](/chrome/browser/android/webapk/AGENTS.md)
- [Android Java Webapps](/chrome/android/java/src/org/chromium/chrome/browser/webapps/AGENTS.md)
- [Blink Manifest Parser](/third_party/blink/renderer/modules/manifest/AGENTS.md)
- [Blink Manifest Common](/third_party/blink/public/common/manifest/AGENTS.md)
- [Content Manifest](/content/browser/manifest/AGENTS.md)

## Architectural Invariants

- **`WebappsClient` Delegate Pattern:** `components/webapps/` must never include
  `+chrome/` headers. Delegate platform-specific operations to Android or
  Desktop embedders via [`WebappsClient`](browser/webapps_client.h).
- **App Service Decoupling:** Do NOT add new dependencies on
  `//components/services/app_service` or `apps::` types. Always define or use
  `webapps::` types.

## Testing & Verification

- **Shared Components Unit Tests:**
  `tools/autotest.py -C out/Default components/webapps/browser/installable/installable_evaluator_unittest.cc`
- **Android JUnit (Robolectric):**
  `autoninja -C out/Android chrome_junit_tests && out/Android/bin/run_chrome_junit_tests -f "org.chromium.chrome.browser.webapps.*"`
- **Android Instrumentation:**
  `autoninja -C out/Android chrome_public_test_apk && out/Android/bin/run_chrome_public_test_apk -f "org.chromium.chrome.browser.webapps.*"`

## Primary Agent & Skills

- **Agent:** `webapps_agent`
  ([`_agents/agents/webapps_agent.md`](_agents/agents/webapps_agent.md))
- **Skills:**
  - `webapps-harness` (`_agents/skills/webapps-harness/`): Load project context
    and navigate architecture.
  - `harness-doc-writer` (`_agents/_harness/skills/harness-doc-writer/`): Author
    designs and execution plans with adversarial review.
  - `harness-updater` (`_agents/_harness/skills/harness-updater/`): Audit link
    integrity, freshness, and spatial maps.
