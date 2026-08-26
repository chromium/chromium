# Web Apps Shared Component (`components/webapps`)

This directory contains features and logic for Web Applications (Progressive Web
Apps) that are shared across platforms (primarily Android and Desktop).

Following Chromium's
[documentation guidelines](/docs/documentation_guidelines.md), this document
focuses on **how shared subsystems interact, data flows, and architectural
layering**. For per-class API and method details, refer to the respective header
files.

For platform-specific implementations and embedder orchestrations, see:

- [Desktop Web Apps (`chrome/browser/web_applications`)](/chrome/browser/web_applications/README.md)
- [Desktop UI Integration (`chrome/browser/ui/web_applications`)](/chrome/browser/ui/web_applications/README.md)
- [Android Web Apps (WebAPKs and TWAs) Architecture](docs/android_architecture.md)
- [Web Apps Core Concepts](/docs/webapps/README.md) - Universal and
  cross-platform Progressive Web App documentation.

## Architectural Layering & Delegate Pattern

Code inside `components/webapps/` must **never** depend on embedder code in
`chrome/` directly.

To communicate back to platform embedders (e.g.
`chrome/browser/web_applications/` on Desktop or `chrome/android/` on Android),
this component defines abstract delegate interfaces:

- [`WebappsClient`](browser/webapps_client.h): Singleton delegate interface
  implemented by the embedder (`ChromeWebappsClient` in
  `chrome/browser/webapps/`). Provides embedder hooks for security state,
  banner/infobar management, install sources, ML segmentation, and conflict
  detection.

## Core Subsystems & Interaction Flow

The shared component orchestrates the pipeline from page discovery to
installation promotion:

```
Active WebContents
         │
         ▼
[1. Page Metadata]           components/webapps/renderer/ (HTML fallback icons & meta tags)
         │
         ▼
[2. Installability Check]    InstallableManager (browser/installable/)
                             ├── Coordinates manifest evaluation & icon verification
                             └── Delivers InstallableData to observers
                                       │
                                       ▼
[3. Promotion & Throttling]  AppBannerManager (browser/banners/)
                             ├── Evaluates engagement & dismissal rate limiting
                             └── Dispatches promotion via WebappsClient delegate
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
     Desktop PWA Promotion                 Android Promotion
  PwaInstallPageAction (Omnibox)       Ambient Badge / Bottom Sheet / WebAPK
```

### 1. Installability Pipeline (`browser/installable/`)

- **[`InstallableManager`](browser/installable/installable_manager.h)**
  coordinates asynchronous checks across the page: evaluating manifest validity,
  verifying icon sizes/codecs, and checking display modes.
- Packages results into an
  **[`InstallableData`](browser/installable/installable_data.h)** struct
  delivered to observers.

### 2. Promotion & Banner Management (`browser/banners/`)

- **[`AppBannerManager`](browser/banners/app_banner_manager.h)** subscribes to
  `InstallableManager` to determine if and when an install prompt should be
  presented to the user.
- Consults
  **[`AppBannerSettingsHelper`](browser/banners/app_banner_settings_helper.h)**
  to check engagement thresholds and enforce dismissal/ignore rate limiting so
  users are not spammed.
- Dispatches UI presentation requests to platform-specific embedder coordinators
  via `WebappsClient`.

### 3. Identifiers & Common Types (`common/`)

- Defines standard types and hashing algorithms used across all layers:
  - [`ManifestId`](common/manifest_id.h) and
    [`webapps::AppId`](common/web_app_id.h).
  - Cross-platform identifier mapping is detailed in
    [Web App Identifiers](docs/identifiers.md).

### 4. Shared Isolated Web Apps Utilities (`isolated_web_apps/`)

- Common utilities, bundle validation, and signing types for Isolated Web Apps
  (IWAs) shared across components.

## Android Subsystems & Deep Dives

- [Android Web Apps Architecture (WebAPKs and TWAs)](docs/android_architecture.md)
- [Registration and Permission Delegation](docs/android_registration_and_permissions.md)
- [TWA Launch Parameters Handling](docs/android_twa_launch_params.md)
- [Android Testing Guide](docs/android_testing_guide.md)
