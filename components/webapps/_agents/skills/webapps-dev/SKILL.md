---
name: webapps-dev
description: >-
  How to develop, build, and test WebApps / PWA features across Desktop and
  Android. This includes installation & related features, TWAs, WebAPKs,
  WebShare, etc.
---

# WebApps Development & Testing

Operational procedures, test runner examples, and development workflows for the
WebApps ecosystem.

## Project Context, Rules, and Invariants

- **Central Hub:** [AGENTS.md](/components/webapps/AGENTS.md)
- **Directory Layout & Layering:**
  [CODE_STRUCTURE.md](/components/webapps/_agents/CODE_STRUCTURE.md)

## Testing Procedures & Gotchas

Always prefer `tools/autotest.py` because it automatically maps source files to
GN targets, compiles with ninja, and runs tests with proper filters.

> [!WARNING] **Gotcha (Stale Builds):** If you invoke manual test runners
> (`run_chrome_public_test_apk` or `run_chrome_junit_tests`) instead of
> `autotest.py`, you **must compile first** via `autoninja`. Running the script
> without building will execute stale bytecode or fail.

### 1. Desktop Tests

- **Unit Tests:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/web_applications/<path>_unittest.cc
  ```
- **Browser Tests:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/ui/web_applications/<path>_browsertest.cc
  ```
- See [Desktop Testing Guide](/chrome/browser/web_applications/docs/testing.md)
  for test fixtures and flags.

### 2. Android Tests

- **Host-Side JUnit (Robolectric):** Runs on host JVM (no emulator required):
  ```bash
  tools/autotest.py -C out/Android \
      chrome/android/junit/src/org/chromium/chrome/browser/webapps/<Test>.java
  # Or, fallback to manual build and run:
  autoninja -C out/Android chrome_junit_tests && \
      out/Android/bin/run_chrome_junit_tests -f "<Filter>"
  ```
- **Instrumentation Tests (`chrome_public_test_apk`):** Runs on device/emulator.
  - **Device Pre-check:** Run `adb devices`. If no device is listed, ask the
    user to start an emulator:
    ```bash
    tools/android/avd/avd.py start -v \
      --avd-config tools/android/avd/proto/android_33_google_apis_x64_local.textpb \
      --emulator-window --no-read-only --enable-network
    ```
  - **Run:**
    ```bash
    tools/autotest.py -C out/Android \
        chrome/android/javatests/src/org/chromium/chrome/browser/webapps/<Test>.java
    # Or, fallback to manual build and run:
    autoninja -C out/Android chrome_public_test_apk && \
        out/Android/bin/run_chrome_public_test_apk -f "<Filter>"
    ```
  - See
    [Android Testing Guide](/components/webapps/docs/android_testing_guide.md)
    for complete suites.

### 3. Shared Components Unit Tests

```bash
tools/autotest.py -C out/Default components/webapps/browser/installable/installable_evaluator_unittest.cc
```

## Design & Review Workflows

- For non-trivial designs or execution plans, use `harness-doc-writer` and
  coordinate reviews with `chromium_design_reviewer`.
- After structural changes or new directories, audit links and maps with
  `harness-updater`.
