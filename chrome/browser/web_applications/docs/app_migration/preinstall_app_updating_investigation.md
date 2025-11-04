# Investigation: Preinstalled App Update-on-Failure Mechanisms

- **Author(s):** dmurph@google.com
- **Team:** Web Apps
- **Status:** In Progress
- **Last modified:** 2025-10-27
- **Tracking Bug:** https://crbug.com/450002138, https://crbug.com/452416687
- **Design Doc:** [link](preinstalled_app_updating_design.md)

This document is not intended to be reviewed by a code reviewer, but instead
document the investigation and design of this feature. It can be referenced if
there are questions.

## Subject

This investigation explores robust update mechanisms for pre-installed web apps,
motivated by the planned migration of the Google Chat app. The pre-installed
Google Chat app is changing its origin from `mail.google.com/chat` to
`chat.google.com`, managed by the `kWebAppMigratePreinstalledChat` feature flag.

The primary problem is ensuring a smooth transition for users who have the older
version of the app installed. When a user launches the old app (scoped to
`mail.google.com/chat`), the page redirects to the new `chat.google.com` origin.
Before the old app's manifest is updated to include `chat.google.com` in its
`scope_extensions`, this new URL is considered out-of-scope. This causes the app
window to display an 'out-of-scope' toolbar or banner, degrading the standalone
app experience. The regular manifest update check will not run because the user
is no longer in the app's scope. We need a mechanism to trigger a background
update check for the app's original manifest (`mail.google.com/chat`) so that it
can discover the new `scope_extensions` and correctly handle the new URL within
the app window.

### Discovered Key Considerations
-   **Reliability:** The update mechanism for "old" pre-installed apps needs to
    be reliable to ensure users get the latest version, which might include
    important changes like scope extensions.
-   **User Data:** Pre-installed app migrations should not be forced on the user
    to avoid losing user-migrated data like permissions.
-   **Cross-Origin Complexity:** Manifest updates in extended scopes are complex
    because the manifest URL might not be on the same origin as the installed
    app, making it difficult to verify the update.
-   **Throttling:** Any new update mechanism must be throttled to avoid
    excessive network requests and processing, especially since it might be
    triggered by common user actions like navigation. A one-day throttle seems
    reasonable.
-   **Manifest Identity:** The manifest parsing logic requires that the `id` in
    the manifest resolves to an origin that matches the document's origin where
    the manifest is being parsed. This means it's not possible to use a manifest
    from an extended scope's origin (e.g., `chat.google.com`) to update an app
    whose identity is tied to the main origin (e.g., `mail.google.com`). Any
    update check *must* be performed against the original install URL.
-   **`FetchManifestAndUpdate` Behavior:** The `FetchManifestAndUpdate` command
    uses a background `WebContents` to fetch a known, non-redirecting
    `install_url`. This existing mechanism is well-suited for our needs, as it
    sidesteps the complexities of parsing manifests on cross-origin pages. The
    challenge is not *how* to perform the update, but *when* to trigger it.

### Discovered Ideas

1.  **Status Quo: Random Delayed Task on Startup.** Stick with the current
    implementation, which runs a delayed background update task on startup. This
    is the baseline to compare against.

2.  **`WebAppTabHelper` Trigger on Extended Scope Navigation.** When a user
    navigates into an *extended scope* of a preinstalled app, trigger the
    background update against the app's original install URL. This is a general
    mechanism that could be implemented in `WebAppTabHelper`.

3.  **`ManifestUpdateManager` Special Case for Extended Scope.** Introduce a
    special case in `ManifestUpdateManager`. The trigger would be a navigation
    to an extended scope where the origin does not match the app's `start_url`
    origin, for a preinstalled app. The action would be to trigger a background
    update against the app's original install URL.

4.  **Redirect-Aware Trigger in `WebAppTabHelper`.** This is the most refined
    idea so far. The trigger would be detecting a completed navigation that was
    redirected from an in-scope URL of a preinstalled app to an out-of-scope URL.
    This would be implemented by observing `content::NavigationHandle` in
    `WebAppTabHelper` and would trigger an update against the app's original
    install URL.

5.  **Chat-Specific Heuristic.** Instead of a general framework, implement a
    solution specifically for the Google Chat preinstalled app. This would allow
    for more tailored logic, such as only triggering the update if the app's
    `scope_extensions` are not yet registered.

6.  **Update-on-Failure in Extended Scopes (Invalidated).** Intentionally allow
    manifest updates to be checked in extended scopes and use the failure as a
    trigger. This idea is invalidated by the "Manifest Identity" consideration;
    the check would fail due to a fundamental identity mismatch, not a transient
    error, making this approach incorrect.

## Summary of Findings and Proposed Approach

The investigation confirms that a robust update trigger is feasible and necessary.

-   **Findings:**
    1.  `WebAppTabHelper` is the correct place to observe navigation events for any given tab, as it is a per-`WebContents` object.
    2.  `content::NavigationHandle` provides the necessary `GetRedirectChain()` method to reliably detect the specific in-scope to out-of-scope redirect that motivates this feature.
    3.  `AppBrowserController` is *not* suitable for observing navigations on all tabs within a window, as its `WebContentsObserver` role is limited to the active tab. It is, however, the authority for window-level state.

-   **Proposed Approach:**
    Based on these findings, the proposed solution is a **one-shot update trigger** that fires only on the initial navigation within a newly created app window. This approach requires collaboration between the window-level `WebAppBrowserController` and the per-tab `WebAppTabHelper`. The controller will identify the initial tab, and that tab's helper will be responsible for checking its first navigation for the specific redirect scenario. This design correctly separates window-level and tab-level concerns and precisely targets the failure case without creating an overly broad or complex mechanism. This will be implemented alongside the periodic startup check to provide a comprehensive solution.

### Testing Plan

-   A unit test will be added to
    `//chrome/browser/web_applications/preinstalled_web_app_manager_unittest.cc`
    to verify the startup update logic. This test will confirm that the update
    is correctly throttled by triggering it twice and ensuring the second
    attempt is blocked.
-   Two new tests will be added to
    `//chrome/browser/web_applications/preinstalled_web_app_manager_unittest.cc`
    for the navigation-based trigger:
    1.  A test that simulates a redirect from an in-scope `start_url` to an
        out-of-scope URL and verifies that the update is triggered.
    2.  A test that simulates the app being launched directly with an
        out-of-scope URL and verifies the update is triggered.

## Background

The work in crbug.com/450002138 and crbug.com/452416687 is motivated by the
migration of the Google Chat pre-installed app. The key problem is ensuring that
all users, especially inactive ones, receive a critical manifest update before a
server-side change breaks their app experience.

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
guarantee these inactive users are updated in time. This investigation therefore
explores more robust triggers, with a **periodic startup check** being the
primary mechanism to protect inactive users, and a **navigation-based check**
serving as a fallback for active users.


## Investigation

### Step 1 (2025-10-24): Analyze `WebAppTabHelper` and `ManifestUpdateManager`

**Questions:**

-   Does `WebAppTabHelper` trigger manifest updates for pages in extended
    scopes?
-   How does `ManifestUpdateManager` handle manifest checks for different
    scopes?
-   What are the potential failure modes for manifest updates in extended
    scopes?

**Findings:**

-   `WebAppTabHelper::PrimaryPageChanged` is called on navigation and is
    responsible for associating a `WebContents` with a web app. It uses
    `WebAppRegistrar::FindBestAppWithUrlInScope` to do this.
-   `FindBestAppWithUrlInScope` *does* consider extended scopes when finding the
    best app for a given URL. It uses
    `WebAppRegistrar::GetAppExtendedScopeScore` to calculate a score based on
    the URL's relationship to the app's scope.
-   However, `WebAppTabHelper::PrimaryPageChanged` also calls
    `ManifestUpdateManager::MaybeUpdate`.
-   `ManifestUpdateManager::MaybeUpdate` has a check:
    `provider_->registrar_unsafe().GetUrlInAppScopeScore(url, app_id.value()) ==
    0`. If this is true, it exits early.
-   `GetUrlInAppScopeScore` explicitly *excludes* extended scopes from its
    calculation.
-   **Conclusion:** The system correctly identifies that a tab is in an app's
    extended scope, but the manifest update check is deliberately suppressed in
    this case. This provides a clear opportunity to implement a custom update
    mechanism for preinstalled apps in this scenario.

### Step 2 (2025-10-24): Investigate Redirect Detection

**Questions:**

-   How can we detect a navigation that has been redirected?
-   Can we access the full redirect chain from within `WebAppTabHelper`?

**Findings:**

-   `content::NavigationHandle`, which is available in several
    `WebContentsObserver` methods like `DidFinishNavigation` and
    `PrimaryPageChanged`, has a `GetRedirectChain()` method.
-   This method returns a `std::vector<GURL>` containing the full redirect
    chain. The first element is the initial URL, and the last is the final URL.
-   A redirect can be detected by checking if `GetRedirectChain().size() > 1`.

**Conclusion:** It is feasible to implement the "Redirect-Aware Trigger" (Idea
4) by inspecting the `NavigationHandle` in `WebAppTabHelper`.

### Step 3 (2025-10-27): Analyze `AppBrowserController` Observation Scope

**Questions:**

-   How does `AppBrowserController` observe navigations across all tabs in an app window?
-   Can it reliably detect the first navigation *in the window* even if it occurs in a background tab?

**Findings:**

-   `AppBrowserController` is both a `TabStripModelObserver` and a `content::WebContentsObserver`.
-   As a `TabStripModelObserver`, it is notified of all tab insertions and removals via `OnTabInserted` and `OnTabRemoved`.
-   However, its role as a `content::WebContentsObserver` is explicitly limited to the **active tab only**. The key code is in `AppBrowserController::OnTabStripModelChanged`: when the active tab changes, it calls `content::WebContentsObserver::Observe(selection.new_contents)`.
-   A `WebContentsObserver` can only observe a single `WebContents` instance at a time. This call directs all navigation-related event listening (like `DidFinishNavigation`) to the newly activated tab and implicitly stops listening to the previous one.
-   This means `AppBrowserController` is blind to navigation events happening in any background tab. If an app opens and the initial navigation completes in a tab before it becomes active, the controller will miss that event entirely.

**Conclusion:** The initial idea of placing the "first navigation" detection logic solely within `WebAppBrowserController` is flawed. It cannot reliably detect the first navigation in the window because it does not observe events in background tabs. The logic to detect an out-of-scope navigation must reside in a per-tab observer like `WebAppTabHelper`. `WebAppBrowserController`'s role should be limited to managing window-level state, such as ensuring an update check is triggered only once per window instance.

### Step 4 (2025-10-27): Investigate Throttling Mechanisms

**Questions:**

-   What are the requirements for throttling these new update triggers?
-   Are there existing, reusable throttling systems within web apps?

**Findings:**

-   **Requirement:** The throttling mechanism must be persistent across browser
    restarts. This is critical for the periodic startup check to ensure it
    doesn't run on every startup, and it's important for the navigation check
    to respect the user's machine over time.
-   **`ManifestUpdateManager`:** This system has its own internal, in-memory
    throttling to prevent repeated update checks for the same app within a
    short time frame. However, because it is in-memory, it does not persist
    across restarts and is therefore unsuitable for our needs.
-   **`WebAppPrefGuardrails`:** This is a persistent, pref-based system
    designed for exactly this type of scenario: limiting how often an action
    can be taken based on user behavior or time.
    -   It supports time-based muting on a per-app basis (e.g., "do not perform
        this action for app X if it was already done in the last 7 days").
    -   It is configurable and can be set up to never permanently block an
        action, which is required for a periodic check.
    -   The system is already used for the existing preinstalled app startup update
        check, setting a clear precedent.

**Conclusion:** `WebAppPrefGuardrails` is the correct and existing mechanism to
use for throttling both the startup and the new navigation-based update
triggers. A new configuration will be needed for the navigation-based trigger to
keep its state separate from the startup trigger.

### Step 5 (2025-10-27): Refine Trigger Scope

**Questions:**

-   Should the update trigger fire on *any* out-of-scope navigation within an
    app window, or only under more specific circumstances?
-   What are the potential downsides of an overly broad trigger?

**Findings:**

-   An initial proposal considered triggering the update check every time a tab
    navigated out of scope.
-   **Downside Analysis:** This was determined to be too aggressive. While the
    update command itself is throttled, the logic to check the navigation would
    run on every out-of-scope navigation. This is unnecessary complexity and
    could have unforeseen performance implications or interactions with other app
    features.
-   **Refinement:** The core problem we are solving is a failure case that
    happens immediately upon app launch (a redirect to a new, out-of-scope
    origin). A user navigating out-of-scope later in the app's lifecycle is a
    different, and expected, user action.
-   **Conclusion:** The trigger mechanism should be narrowed to a **one-shot**
    check that only targets the **initial navigation** in a new app window. This
    precisely targets the bug we are trying to fix without adding unnecessary,
    continuous checks, resulting in a simpler, safer, and more performant
    solution.

## Summary of Findings and Proposed Approach

The current implementation correctly identifies when a user navigates into an
extended scope of an installed web app. However, it is designed to *prevent*
manifest updates in this scenario. This is a deliberate choice, likely to avoid
issues with cross-origin manifest fetching. This behavior provides a clear and
logical place to hook in a new update mechanism for pre-installed apps. The
`content::NavigationHandle` provides the necessary information to detect the
specific redirect scenario that is the core of the problem. The manifest
identity constraint confirms that any update must be triggered against the app's
original install URL.

## Relevant Context

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

## Document History

| Date       | Summary of changes                                              |
| -----------|---------------------------------------------------------------- |
| 2025-10-24 | Initial document creation, outlining the problem and potential solutions. |
| 2025-10-24 | Added new ideas and refined the problem description based on user feedback. |
| 2025-10-24 | Added all discussed ideas to the 'Discovered Ideas' section and added a new step to the 'Investigation' section. |
| 2025-10-24 | Added manifest identity constraint and invalidated Idea 6. |
| 2025-10-24 | Added Proposed Solution section, incorporating the implementation and testing plan. Updated Relevant Context with full file paths. |
| 2025-10-24 | Refined Subject to be specific to the Google Chat app migration. |
| 2025-10-24 | Corrected inaccurate description of out-of-scope navigation behavior. |
| 2025-10-24 | Clarified that inactive users are the primary motivation for the feature. |
| 2025-10-27 | Investigated `AppBrowserController` observation behavior and found it unsuitable for the initial design. Refined the proposed approach to a collaboration between the controller and `WebAppTabHelper`. Added investigation steps for controller behavior, throttling, and trigger scope refinement. |
