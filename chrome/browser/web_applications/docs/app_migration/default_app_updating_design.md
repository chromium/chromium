# Design Doc: Default App Update Triggers

- **Author(s):** dmurph@google.com
- **Team:** Web Apps
- **Status:** Draft
- **Last modified:** 2025-10-24
- **Tracking Bug:** https://crbug.com/450002138, https://crbug.com/452416687
- **Investigation Doc:** [Investigation: Default App Update-on-Failure
  Mechanisms](./default_app_updating_investigation.md)

This document is not intended to be reviewed by a code reviewer, but instead
document the investigation and design of this feature. It can be referenced if
there are questions.

## Context

### Objective
To ensure pre-installed web apps, like the Google Chat app, can be reliably
updated in the background to support origin migrations without disrupting the
user or requiring a forced reinstall, which would lose user data.

### Background
The pre-installed Google Chat app is migrating from `mail.google.com/chat` to
`chat.google.com`. This design proposes robust update triggers to handle this
migration smoothly, with the primary goal of protecting inactive users from a
broken experience.

The migration involves two phases:
1.  **Manifest Update:** The Chat app's manifest is updated to include
    `scope_extensions` for the new `chat.google.com` origin. Active users who
    launch the app will receive this update via the standard manifest update
    process. This solves the in-app redirect problem for them, where they would
    otherwise see an out-of-scope toolbar.
2.  **Server-Side Redirect Change:** Sometime after the manifest update is
    available, `chat.google.com` links will stop redirecting to
    `mail.google.com/chat`.

The critical failure case is for **inactive users** who do not open the Chat app
between these two phases. Their app will not have received the manifest update.
When the server-side change occurs, their link-capturing functionality will
break; `chat.google.com` links will open in a browser tab instead of the app.

The existing delayed-startup task for updates is not reliable enough to
guarantee these inactive users are updated in time. This design therefore
proposes a **periodic startup check** as the primary mechanism to protect
inactive users, with a **navigation-based check** serving as a fallback for
active users who might still encounter the redirect.

### Platforms
This design affects all desktop platforms where pre-installed web apps are
supported: Mac, Windows, Linux, Chrome OS.

## Design

### Overview
This design introduces two new, throttled update triggers for default-installed
web apps to ensure they receive timely manifest updates, particularly to handle
origin changes.

1.  **Periodic Startup Update:** On browser startup, a task will check for
    default apps that require an update and trigger a manifest update check,
    throttled to once every 7 days.
2.  **Navigation-based Update:** When a default app is launched, if the first
    navigation within the app window completes at an out-of-scope URL, a
    manifest update check will be triggered. This is also throttled to once
    every 7 days.

Both triggers will use the existing `FetchManifestAndUpdate` command, which is
designed for safe, trusted background updates.

### Code Affected
-   `WebAppProvider`: To schedule the startup update check.
-   `WebAppBrowserController`: To implement the navigation-based update trigger.
-   `WebAppPrefGuardrails`: To provide persistent, daily throttling for these
    new triggers.
-   `PreinstalledWebAppManager`: The source of truth for which apps are
    default-installed.

### Detailed Design

#### 1. Startup Update Trigger
In `WebAppProvider::StartImpl()`, after subsystems are started, a new method
`SchedulePeriodicUpdateChecks()` will be called. This method will:
-   Iterate through all installed default apps.
-   For each app, check if a throttled update is allowed using a new
    `WebAppPrefGuardrails` configuration (see Throttling section below).
-   If an update is allowed, it will call
    `provider_->scheduler()->ScheduleFetchManifestAndUpdate()` with the app's
    install URL.
-   It will then record that an update check was performed to reset the
    throttle.

#### 2. Navigation-based Update Trigger
The `WebAppBrowserController` will be modified to detect out-of-scope
navigations on launch.
-   A new boolean member, `did_first_navigation_in_window_complete_`, will be
    added and initialized to `false`.
-   `WebAppBrowserController` will observe navigations in its `WebContents`.
-   In `DidFinishNavigation`, if `did_first_navigation_in_window_complete_` is
    `false`, it will be set to `true`.
-   It will then check if the app is a default install and if the navigation's
    final URL is outside the app's scope.
-   If both are true, it will check the `WebAppPrefGuardrails` throttle. If
    allowed, it will trigger `ScheduleFetchManifestAndUpdate()` and record the
    check.

#### 3. Throttling
To avoid excessive network requests, both triggers will be throttled to
approximately once every 7 days.
-   A new `GuardrailData` constant will be defined in
    `web_app_pref_guardrails.h` for this purpose, named
    `kDefaultAppUpdateOnStartupGuardrails`, configured with a 7-day mute
    duration via `global_mute_after_ignore_days`.
-   To ensure the update check is truly periodic and is never permanently
    disabled, the `app_specific_not_accept_count` and `global_not_accept_count`
    will be explicitly set to `std::nullopt`. This disables the guardrail's
    feature of blocking a prompt after a certain number of ignores.
-   When an update check is triggered, the code will call
    `WebAppPrefGuardrails::RecordIgnore()` for the app ID. This co-opts the
    "ignore" mechanism to mean "an update check was attempted".
-   Before triggering an update, `WebAppPrefGuardrails::IsBlockedByGuardrails()`
    will be called to respect the throttle.

## Alternatives Considered

### Alternative A: `ManifestUpdateManager` Throttling
-   **Description:** Use the existing throttling mechanism in
    `ManifestUpdateManager`.
-   **Pros:** Semantically closer to the feature.
-   **Cons:** The existing throttle is in-memory only and does not persist
    across browser restarts, making it unsuitable for the periodic startup
    check.
-   **Reason for not choosing:** Lack of persistence.

### Alternative B: New Pref-based Throttling System
-   **Description:** Build a new, simple throttling system specifically for this
    feature that persists last-check timestamps to Prefs.
-   **Pros:** Would be a cleaner, more semantically correct API than co-opting
    `WebAppPrefGuardrails`.
-   **Cons:** Involves writing new code for a problem that is already solved by
    `WebAppPrefGuardrails`.
-   **Reason for not choosing:** Pragmatically, reusing the existing, tested
    `WebAppPrefGuardrails` is faster and less risky than implementing a new
    system.

## Quality Attributes

### Speed
The navigation-based check adds a small amount of work (`IsDefaultApp`,
`IsInScope`) to each first navigation in an app window. This is negligible. The
startup check is a low-priority background task. Both update fetches are
throttled to once every 7 days. The performance impact is expected to be
minimal.

### Security
The update process uses `FetchManifestAndUpdateCommand`, which is designed for
trusted app updates. It verifies the manifest's identity against the app's known
ID. No new security risks are introduced.

### Privacy
The update triggers a network request to the app's install URL (e.g.,
`mail.google.com`). This reveals that the pre-installed app is active on the
user's machine. This is considered an acceptable privacy impact as the app is
bundled with the browser.

## Testing Plan
-   **Startup Trigger:** A unit test will be added to
    `preinstalled_web_app_manager_unittest.cc` to verify the startup update
    logic. This test will confirm the update is correctly throttled by
    triggering it twice and ensuring the second attempt is blocked.
-   **Navigation Trigger:** Browser tests will be added to the
    `WebAppBrowserControllerBrowserTest` suite.
    1.  A test will simulate a redirect from an in-scope `start_url` to an
        out-of-scope URL and verify that `ScheduleFetchManifestAndUpdate` is
        called.
    2.  A test will simulate launching the app directly into an out-of-scope URL
        and verify the update is triggered.
    3.  A test will verify that the navigation trigger is also correctly
        throttled.

## Relevant Key Files and Context

*   **Files:**
    *   `//chrome/browser/web_applications/preinstalled_web_apps/google_chat.cc`
    *   `//chrome/browser/web_applications/web_app_tab_helper.cc`
    *   `//chrome/browser/web_applications/manifest_update_manager.cc`
    *   `//content/public/browser/navigation_handle.h`
    *   `//chrome/browser/web_applications/web_app_pref_guardrails.h`
    *   `//chrome/browser/ui/web_applications/web_app_browser_controller.h`
    *   `//chrome/browser/web_applications/preinstalled_web_app_manager_unittest.cc`
*   **Key Bugs:**
    *   https://crbug.com/450002138
    *   https://crbug.com/452416687
