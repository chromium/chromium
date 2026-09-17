# Web Applications Central Hub (components/webapps)

Central Hub for Progressive Web Apps (PWAs), WebAPKs, Isolated Web Apps (IWAs),
and Web App Manifest processing in Chromium.

[Parent Harness Rules](_agents/_harness/AGENTS.md)

## Navigation

- **Maps & Structure:** [Code Structure](_agents/CODE_STRUCTURE.md)
- **Docs:** [Overview](README.md) |
  [Android Testing](docs/android_testing_guide.md) |
  [Desktop Testing](/chrome/browser/web_applications/docs/testing.md)

## Satellite Spokes

- [Desktop Backend](/chrome/browser/web_applications/AGENTS.md)
- [Desktop UI](/chrome/browser/ui/web_applications/AGENTS.md)
- [Desktop Views UI](/chrome/browser/ui/views/web_apps/AGENTS.md)
- [Android Native WebAPK](/chrome/browser/android/webapk/AGENTS.md)
- [Android Java Webapps](/chrome/android/java/src/org/chromium/chrome/browser/webapps/AGENTS.md)
- [Blink Manifest Parser](/third_party/blink/renderer/modules/manifest/AGENTS.md)
- [Blink Manifest Common](/third_party/blink/public/common/manifest/AGENTS.md)
- [Content Manifest](/content/browser/manifest/AGENTS.md)

## Gotchas & Invariants

- **App Service Decoupling:** Do NOT add new dependencies on
  `//components/services/app_service` or `apps::` types. Always define or use
  `webapps::` types.

## Verification & Testing

- Prefer `tools/autotest.py` for test compilation and execution.
- Before running Android instrumentation tests (`chrome_public_test_apk`), check
  `adb devices`. If no online device or emulator is active, pause and ask the
  user to start an emulator.
- For copy-pasteable test targets, commands, and recipes, load skill
  `webapps-dev`.
