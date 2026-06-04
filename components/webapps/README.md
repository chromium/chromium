# Web Apps Shared Component (`components/webapps`)

This directory contains features and logic for Web Applications (Progressive Web Apps) that are shared between different platforms (primarily Android and Desktop).

For platform-specific implementations and orchestrations, see:
*   [Desktop Web Apps (`chrome/browser/web_applications`)](../../chrome/browser/web_applications/README.md)
*   [Android Web Apps (WebAPKs and TWAs) Architecture](docs/android_architecture.md)
*   [Web Apps Core Concepts](../../docs/webapps/README.md) - Universal and cross-platform Progressive Web App documentation.

## Key Subsystems

### 1. Installability Detection (`browser/installable/`)

The `InstallableManager` is responsible for checking if a website is installable as a Progressive Web App. It verifies:
*   Presence and validity of a Web App Manifest.
*   Service worker registration (if required).
*   Icon requirements.
*   Other installability criteria.

See header files for details:
*   [`InstallableManager`](browser/installable/installable_manager.h) - Main entry point to schedule installability checks.
*   [`InstallableData`](browser/installable/installable_data.h) - Structure containing the results of the check.

### 2. App Banners and Promotion (`browser/banners/`)

The `AppBannerManager` decides when and how to promote PWA installation to the user (e.g., showing an install icon in the omnibox, an ambient badge, or a bottom sheet). It uses `InstallableManager` to check eligibility and tracks user engagement to avoid spamming.

See header files for details:
*   [`AppBannerManager`](browser/banners/app_banner_manager.h) - Base class for managing banner visibility and triggering.
*   [`AppBannerSettingsHelper`](browser/banners/app_banner_settings_helper.h) - Tracks history of banner events (shown, ignored, dismissed) to enforce rate limiting.

### 3. Identifiers

Web apps use specific identifiers to track installations and partition data.
*   See [Web App Identifiers](docs/identifiers.md) for details on Manifest ID, App ID, and platform-specific identifiers.

## Deep Dives (Android-specific)

*   [Registration and Permission Delegation](docs/android_registration_and_permissions.md)
*   [Android Testing Guide](docs/android_testing_guide.md)

## Documentation Guidelines

Following the model of [Desktop Web Apps README](../../chrome/browser/web_applications/README.md):
*   **Markdown documentation** (in `docs/`) should focus on high-level architecture, cross-class interactions, and concepts.
*   **Class-level documentation** (in header files) should document the responsibilities of individual classes and their APIs.
