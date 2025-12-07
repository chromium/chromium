# Design Doc: Preinstalled App Update Triggers

- **Author(s):** dmurph@google.com
- **Team:** Web Apps
- **Status:** Done
- **Last modified:** 2025-10-30
- **Tracking Bug:** https://crbug.com/450002138, https://crbug.com/452416687
- **Investigation Doc:** [Investigation: Preinstalled App Update-on-Failure
  Mechanisms](./preinstalled_app_updating_investigation.md)

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
This design introduces two new, throttled update triggers for preinstalled
web apps to ensure they receive timely manifest updates, particularly to handle
origin changes.

1.  **Periodic Startup Update:** On browser startup, a task will check for
    preinstalled apps that require an update and trigger a manifest update check,
    throttled to once every 7 days.
2.  **One-Shot Navigation Update:** When a preinstalled app window is first opened,
    if its initial navigation is a server-side redirect from an in-scope URL to
    an out-of-scope URL, a manifest update check will be triggered. This is a
    one-time check per window creation.

Both triggers will use the existing `FetchManifestAndUpdate` command, which is
designed for safe, trusted background updates.

### Code Affected
-   `WebAppProvider`: To schedule the startup update check.
-   `WebAppTabHelper`: To detect out-of-scope navigations.
-   `WebAppBrowserController`: To manage window-level state for the
    navigation-based update trigger.
-   `WebAppPrefGuardrails`: To provide persistent, daily throttling for these
    new triggers.
-   `PreinstalledWebAppManager`: The source of truth for which apps are
    preinstalled.

### Detailed Design

#### 1. Startup Update Trigger
In `WebAppProvider::StartImpl()`, after subsystems are started, a new method
`SchedulePeriodicUpdateChecks()` will be called. This method will:
-   Iterate through all installed preinstalled apps.
-   For each app, check if a throttled update is allowed using a new
    `WebAppPrefGuardrails` configuration (see Throttling section below).
-   If an update is allowed, it will call
    `provider_->scheduler()->ScheduleFetchManifestAndUpdate()` with the app's
    install URL.
-   It will then record that an update check was performed to reset the
    throttle.

#### 2. Navigation-based Update Trigger
The trigger will be a one-time check when an app window is created, implemented
via a collaboration between `WebAppBrowserController` and `WebAppTabHelper`.

-   **In `WebAppBrowserController`:**
    -   A new boolean member, `did_notify_first_tab_`, will be added and
        initialized to `false`.
    -   The `OnTabInserted` method will be overridden. On the first call (when
        `did_notify_first_tab_` is `false`), it will:
        1.  Set `did_notify_first_tab_` to `true`.
        2.  Get the `WebAppTabHelper` for the newly inserted `WebContents`.
        3.  Call a new method on the tab helper,
            `NotifyIsFirstWebContentsInAppWindow()`, passing a
            `base::PassKey<WebAppBrowserController>` to restrict access.

-   **In `WebAppTabHelper`:**
    -   A new public method will be added:
        `NotifyIsFirstWebContentsInAppWindow(base::PassKey<WebAppBrowserController>)`.
    -   A new private boolean member,
        `should_do_backup_update_check_on_next_navigation_complete_`, will be
        added and initialized to `false`. The `Notify` method will set this to
        `true`.
    -   In `DidFinishNavigation`, the helper will check if
        `should_do_backup_update_check_on_next_navigation_complete_` is `true`.
    -   If it is, it will immediately set the flag to `false` to disarm the
        trigger.
    -   Do nothing if the tab is not in an app window.
    -   Do nothing if the tab's in-scope `app_id() == window_app_id()`, as the
        app_id_ is set from an app scop lookup (in `ReadyToCommitNavigation`),
        so we can rely on this check to determine if the current navigation is
        out of scope.
    -   Check the `window_app_id_` against the
        `PreinstalledWebAppManager::preinstalled_app_for_updating()`, which
        contains the app configuration for the one being updated.
    -   If a match, then trigger the update!

#### 3. Throttling
To avoid excessive network requests, the startup trigger will be throttled to
approximately once every 7 days using `WebAppPrefGuardrails`. The trigger
happening on app launch should be rare and specific enough to not need a
trigger.

-   One new `GuardrailData` constants will be defined in
    `web_app_pref_guardrails.h`:
    -   `kDefaultAppUpdateOnStartupGuardrails`: For the startup trigger.
-   It will be configured with a 7-day mute duration via
    `app_specific_mute_after_ignore_days` and will have
    `app_specific_not_accept_count` and `global_not_accept_count` disabled to
    ensure the check is periodic and never permanently blocked.
-   The check for whether the trigger is blocked (`IsBlockedByGuardrails()`)
    will happen immediately before scheduling the `FetchManifestAndUpdate`
    command.
-   The recording of the check (`RecordIgnore()`), which starts the 7-day mute
    period, will be done in the completion callback of the
    `FetchManifestAndUpdate` command. This ensures the throttle is only applied
    after a check is successfully attempted.

## Alternatives Considered

### Alternative A: Logic Solely in `WebAppBrowserController`
-   **Description:** The initial design proposed implementing the entire
    navigation-based trigger within `WebAppBrowserController`. It would track
    the "first navigation in the window" and trigger the update check.
-   **Pros:** Keeps all window-related logic in a single class.
-   **Cons:** Further investigation revealed that `AppBrowserController` (the
    base class) is a `WebContentsObserver` for only the *active* tab in the tab
    strip. This makes reliably detecting the first navigation *in the window*
    complex and racy. For example, if a new tab is opened, the controller
    switches its observation, and the logic to determine which navigation is the
    "first" across all tabs becomes difficult to manage correctly.
-   **Reason for not choosing:** The proposed design, which splits
    responsibility between the `WebAppTabHelper` (for tab-level navigation
    events) and the `WebAppBrowserController` (for window-level state), is more
    robust and has a cleaner separation of concerns.

### Alternative B: `ManifestUpdateManager` Throttling
-   **Description:** Use the existing throttling mechanism in
    `ManifestUpdateManager`.
-   **Pros:** Semantically closer to the feature.
-   **Cons:** The existing throttle is in-memory only and does not persist
    across browser restarts, making it unsuitable for the periodic startup
    check.
-   **Reason for not choosing:** Lack of persistence.

### Alternative C: New Pref-based Throttling System
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
The navigation-based check adds a small amount of work to each navigation that
completes within an app window. This work is negligible (a few boolean checks).
The actual update command is heavily throttled (once per window, and once every
7 days), so the performance impact is expected to be minimal.

### Security
The update process uses `FetchManifestAndUpdateCommand`, which is designed for
trusted app updates. It verifies the manifest's identity against the app's known
ID. No new security risks are introduced.

### Privacy
The update triggers a network request to the app's install URL (e.g.,
`mail.google.com`). This reveals that the pre-installed app is active on the
user's machine. This is considered an acceptable privacy impact as the app is
bundled with the browser.

### Simplicity
This change has no user-visible UI component. Its goal is to *increase*
simplicity by preventing a confusing out-of-scope UI from appearing for users of
the Chat app after the origin migration.

### Stability
The change is low-risk. The update mechanism is heavily throttled and runs on a
well-tested command (`FetchManifestAndUpdate`).

### A11y (Accessibility)
There are no UI changes, so there are no direct accessibility impacts.

### Enterprise
This feature is not expected to have any enterprise impact and will not be
controlled by an enterprise policy.

## Technical Debt
This design reuses the `WebAppPrefGuardrails` system, which is semantically
designed for user-facing prompts. We are co-opting its "ignore" mechanism to
mean "an update check was attempted." While a bespoke throttling system might be
cleaner, reusing the existing, battle-tested system is more pragmatic and lower
risk. This is considered acceptable, low-level technical debt.

## Testing Plan
-   **Startup Trigger:** A unit test will be added to
    `preinstalled_web_app_manager_unittest.cc` to verify the startup update
    logic. This test will confirm the update is correctly throttled by
    triggering it twice and ensuring the second attempt is blocked.
-   **Navigation Trigger:** Browser tests will be added to the
    `preinstalled_web_app_manager_browsertest.cc` suite. A test will simulate a
    redirect from an in-scope `start_url` to an out-of-scope URL and verify that
    `ScheduleFetchManifestAndUpdate` is called.

## Launch Plan

### Rollout
This feature will be enabled by default alongside the feature flag for the Chat
app migration (`kWebAppMigratePreinstalledChat`).

## Follow-up Work
Once the Chat app migration is complete and the old origin is fully deprecated,
this navigation-based trigger can be re-evaluated and potentially removed if it
is no longer needed. The periodic startup check may be worth keeping as a
general-purpose mechanism for ensuring preinstalled apps stay up-to-date.

## Relevant Key Files and Context

*   **Files:**
    *   `//chrome/browser/ui/web_applications/app_browser_controller.h`
    *   `//chrome/browser/ui/web_applications/web_app_browser_controller.cc`
    *   `//chrome/browser/ui/web_applications/web_app_browser_controller.h`
    *   `//chrome/browser/web_applications/manifest_update_manager.cc`
    *   `//chrome/browser/web_applications/preinstalled_web_app_manager_unittest.cc`
    *   `//chrome/browser/web_applications/preinstalled_web_app_manager.h`
    *   `//chrome/browser/web_applications/preinstalled_web_apps/google_chat.cc`
    *   `//chrome/browser/web_applications/web_app_pref_guardrails.cc`
    *   `//chrome/browser/web_applications/web_app_pref_guardrails.h`
    *   `//chrome/browser/web_applications/web_app_provider.cc`
    *   `//chrome/browser/web_applications/web_app_tab_helper.cc`
    *   `//chrome/browser/web_applications/web_app_tab_helper.h`
    *   `//content/public/browser/navigation_handle.h`
*   **Key Bugs:**
    *   https://crbug.com/450002138
    *   https://crbug.com/452416687

## Document history

| Date       | Summary of changes                                              |
| -----------|---------------------------------------------------------------- |
| 2025-10-24 | Initial document creation and draft proposal.                   |
| 2025-10-27 | Major revision based on investigation. Changed the navigation-based trigger from a broad, controller-only implementation to a precise, one-shot mechanism involving both `WebAppBrowserController` and `WebAppTabHelper`. Updated all sections to reflect the new, more robust design. |
