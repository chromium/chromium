# Code Structure: Web Apps Ecosystem

**Parent:** [AGENTS.md](../AGENTS.md)

Physical source directories and entry points across Web Apps. Read the linked
READMEs for architecture details and design docs.

## 1. Shared Components (`components/webapps/`)

- [components/webapps/README.md](/components/webapps/README.md): Cross-platform
  installability, promotion, and identifiers shared by Desktop and Android
  (delegates via `WebappsClient`).
  - `browser/installable/`: Manifest & installability verification.
  - `browser/banners/`: Install prompts, badges, and rate limiting.
  - `browser/android/`: Android-specific shared coordination.
  - `common/`: Mojo interfaces and identifiers.
  - `renderer/`: Renderer metadata extraction.
  - `isolated_web_apps/`: Shared IWA bundle utilities.

## 2. Desktop Web Apps (`chrome/browser/web_applications/`)

- [chrome/browser/web_applications/README.md](/chrome/browser/web_applications/README.md):
  Per-profile `WebAppProvider` engine (commands, locks, DB storage, sync, and OS
  integration).
  - [commands/README.md](/chrome/browser/web_applications/commands/README.md):
    Async command implementations.
  - [jobs/README.md](/chrome/browser/web_applications/jobs/README.md): Reusable
    job sub-components.
  - [locks/README.md](/chrome/browser/web_applications/locks/README.md):
    Fine-grained locking hierarchy.
  - `os_integration/`: Shortcuts, file handlers, and protocol handlers.
- [chrome/browser/ui/web_applications/README.md](/chrome/browser/ui/web_applications/README.md):
  Desktop UI controllers, window management, omnibox actions, navigation
  capturing, and Mac App Shims.
- [chrome/browser/ui/views/web_apps/README.md](/chrome/browser/ui/views/web_apps/README.md):
  Views dialogs, bubbles, custom frame toolbars, and integration test driver.

## 3. Android Web Apps

- [components/webapps/docs/android_architecture.md](/components/webapps/docs/android_architecture.md):
  Android architecture overview and complete Java/C++ call graphs.
- [chrome/android/.../webapps/README.md](/chrome/android/java/src/org/chromium/chrome/browser/webapps/README.md):
  Java activity lifecycles and coordinators.
- [chrome/browser/android/webapk/README.md](/chrome/browser/android/webapk/README.md):
  Browser-process native WebAPK services and sync.

## 4. Manifest Parsing & Extraction

- [third_party/blink/renderer/modules/manifest/README.md](/third_party/blink/renderer/modules/manifest/README.md):
  JSON manifest parsing in untrusted renderer.
- [content/browser/manifest/README.md](/content/browser/manifest/README.md):
  Browser-side manifest fetching and icon downloading coordination.
