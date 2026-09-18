---
name: webapps-dev
description: >-
  How to develop, build, and test WebApps / PWA features across Desktop and
  Android. This includes installation & related features, TWAs, WebAPKs,
  etc.
---

# WebApps Development

Supplementary engineering information for the WebApps/PWA product area.

[Central Hub & Directory Map](/components/webapps/AGENTS.md)

## Testing Procedures & Gotchas

Generally always prefer `tools/autotest.py` because it automatically maps source
files to GN targets, compiles with ninja, runs tests with proper filters, and
parallelizes well.

For cross-platform testing, a good method is to use the `utr` skill, which will
facilitate compiling for the given platform and sending the binary to an
applicable bot.

- Most platforms can cross-compile locally, so first ensure that the platform is
  in the .gclient file.
- Often the local compilation config won't match the utr config 'exactly' (e.g.
  code coverage config, etc). This is usually fine, and you can tell the script
  to continue.

### Android

- Using the android test runners like `run_chrome_public_test_apk` will not
  automatically build the test like autotest.py does. So if you are using that
  test runner, make sure to build first.
- Instrumentation tests (e.g. `chrome_public_test_apk`) run on a device or
  emulator. Run first `adb devices` to check to see if one exists. If not, ask
  the user to start an emulator:
  ```bash
  tools/android/avd/avd.py start -v \
    --avd-config tools/android/avd/proto/android_google_apis_x64_local.textpb \
    --emulator-window --no-read-only --enable-network
  ```
- See [Android Testing Guide](/components/webapps/docs/android_testing_guide.md)
  for complete PWA & Android testing information.

## Design & Review Workflows

- For non-trivial designs or execution plans, use `harness-doc-writer` and
  coordinate reviews with `chromium_design_reviewer`.
- After structural changes or new directories, audit links and maps with
  `harness-updater`.
