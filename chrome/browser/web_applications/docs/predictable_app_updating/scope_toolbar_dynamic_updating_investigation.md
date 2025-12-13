# Investigation: Dynamic Custom Tab Bar Visibility on Web App Scope Change

- **Author(s):** dmurph@chromium.org
- **Team:** PWA
- **Status:** Complete
- **Tracking Bug:** https://crbug.com/434956941
- **Design Doc:**
  [scope_toolbar_dynamic_updating_design.md](scope_toolbar_dynamic_updating_design.md)

This document is for developer-AI collaboration. It is not meant to be reviewed
by a code reviewer, it is too verbose and not helpful. I can be referenced if
that is helpful.

## Subject

This investigation explores the implementation of dynamically updating the
visibility of the custom tab bar in web app windows when the app's scope
changes. Currently, the custom tab bar, which is shown when the content is out
of scope, does not update its visibility when the app's scope is modified,
including changes from extended scopes.

### Discovered Key Considerations

A key risk is ensuring that the UI update triggered by `OnWebAppManifestUpdated`
is not immediately overwritten by an existing call to
`UpdateCustomTabBarVisibility` that might occur during a page load lifecycle.
For example, if a manifest update occurs while a navigation is in progress, we
need to ensure the final state of the toolbar is correct for the page that is
ultimately loaded. A detailed analysis of all call sites is required to mitigate
this risk.

### Discovered Ideas

- Adding scope change notification hook on WebAppRegistrarObserver.
  - Likely a worse version of this would be to have a new observer class, to
    limit the scope bloat of this one. But probably worse that way.
- Adding WebAppBrowserController as a listener to scope changes and updating the
  tab visibility on changes for the current app.
  - Another way would be to have scope changes someout reach out and poke all
    WebAppBrowserControllers to refresh the visibility state based on the
    current url being in scope or not for their app.
- Also - instead of having the the listener have to re-look-up if their url is
  in scope, it would be nice to somehow pass as arguments the new scope. This
  would have to be, I guess, both the regular scope and also the extended scope?
  IT would be nice to package it into a struct, so it doesn't have to change if
  the scope spec changes. Like - `WebAppScope` object that holds scope
  information.

## Background

The custom tab bar in a web app window indicates to the user that they have
navigated outside the defined scope of the web app. The visibility of this bar
is controlled by the `UpdateCustomTabBarVisibility` method.

Web app scope is updated through calls to `SetScope` in
`chrome/browser/web_applications/web_app_install_utils.cc`. These calls are
often initiated by the `WebAppInstallFinalizer`. With the introduction of scope
extensions, the effective scope of a web app can change, and this is managed by
the `WebAppRegistrar`.

The current implementation does not seem to handle dynamic updates of the tab
bar's visibility in response to these scope changes. The proposed solution
involves adding a new observer method to `WebAppRegistrarObserver`, such as
`OnEffectiveScopeChanged(app_id)`, and having the `WebAppBrowserController`
observe this to trigger the visibility update.

## Investigation

### Step 1 (2025-08-20): Initial Investigation and Findings

The investigation began by analyzing the key functions and classes involved.

*   **`UpdateCustomTabBarVisibility` Call Sites:** This function is the ultimate
    target for controlling the toolbar's visibility. It was found to be a
    virtual method on `BrowserWindow`, called by
    `AppBrowserController::UpdateCustomTabBarVisibility`. The
    `AppBrowserController` calls this method in response to tab and navigation
    events (`OnTabStripModelChanged`, `OnReceivedInitialURL`), confirming that
    its updates are not currently tied to scope changes.

*   **`WebAppRegistrar` and `WebAppRegistrarObserver`:** The `WebAppRegistrar`
    is responsible for managing `WebApp` objects, and the
    `WebAppRegistrarObserver` is the established pattern for broadcasting
    changes to other parts of the system. It was confirmed that no observer
    method for scope changes currently exists.

### Step 2 (2025-08-20): Production Call Site Investigation

To understand when scope changes in a production environment, an investigation
into the call sites of `WebApp::SetScope` and
`WebApp::SetValidatedScopeExtensions` was conducted, excluding test files.

*   **`WebApp::SetScope` Call Sites:**
    *   `chrome/browser/web_applications/web_app_install_utils.cc`: Called
        during app installation and updates when populating a `WebApp` from a
        `WebAppInstallInfo` struct.
    *   `chrome/browser/web_applications/web_app_database_serialization.cc`:
        Called when deserializing a `WebApp` from the database on startup or
        after a sync.

*   **`WebApp::SetValidatedScopeExtensions` Call Sites:**
    *   `chrome/browser/web_applications/web_app_install_finalizer.cc`: Called
        after scope extensions have been validated during installation.
    *   `chrome/browser/web_applications/web_app_install_utils.cc`: Also called
        when populating from `WebAppInstallInfo`.
    *   `chrome/browser/web_applications/web_app_database_serialization.cc`:
        Called during deserialization.

These findings show that scope changes are tied to installation, updates, and
sync.

### Step 3 (2025-08-20): Call Chain Analysis

A deeper analysis traced the exact call chain for production scope changes:

1.  The process starts with `WebAppInstallFinalizer::FinalizeInstall` or
    `WebAppInstallFinalizer::FinalizeUpdate`.
2.  These methods call the private helper
    `WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData`.
3.  This helper calls the utility function `web_app::SetWebAppManifestFields` in
    `web_app_install_utils.cc`.
4.  `web_app::SetWebAppManifestFields` is the function that directly calls
    `WebApp::SetScope` and `WebApp::SetValidatedScopeExtensions`.

This confirms that scope modifications are part of the app installation and
update process orchestrated by the `WebAppInstallFinalizer`.

### Step 4 (2025-08-20): Analysis of Existing Visibility Triggers and `WebAppTabHelper`

An analysis of the current toolbar visibility triggers and the role of
`WebAppTabHelper` was performed.

*   **Existing Triggers:** `AppBrowserController::UpdateCustomTabBarVisibility`
    is called from `AppBrowserController::OnTabStripModelChanged`, tying
    visibility re-evaluation to tab and navigation events.
*   **Role of `WebAppTabHelper`:** The `WebAppTabHelper` is a *consumer* of
    scope information. It reads the app's scope from the `WebAppRegistrar`
    during navigation to associate the tab with an app. It does not modify the
    scope itself and therefore does not conflict with the proposed changes.

### Step 5 (2025-08-20): `WebAppBrowserController` Analysis

A deeper investigation into `WebAppBrowserController`, the concrete
implementation for web apps, revealed:

*   **Existing `OnWebAppManifestUpdated` Observer:** `WebAppBrowserController`
    already observes for manifest updates via the `OnWebAppManifestUpdated`
    method. This method is called when `WebAppInstallFinalizer::FinalizeUpdate`
    completes. The controller currently updates the theme and title bar in
    response, but does not re-evaluate the custom tab bar's visibility. This
    presents a direct and minimally invasive path for our solution.

### Step 6 (2025-08-20): Exhaustive `UpdateCustomTabBarVisibility` Call Site Analysis

To fully understand the risk of race conditions, an exhaustive search and
analysis of all `UpdateCustomTabBarVisibility` call sites was performed.

1.  **`AppBrowserController::OnTabStripModelChanged(...)`**
    *   **Location:**
        `chrome/browser/ui/web_applications/app_browser_controller.cc`
    *   **Trigger:** Fires after any navigation or tab change is complete.
    *   **Analysis:** This is the most critical call site. It functions as a
        failsafe, as it will always execute at the end of a navigation. This
        ensures that it will correct the toolbar's visibility based on the
        final, committed URL and the most current app scope, mitigating any race
        conditions that might occur during the navigation itself.

2.  **`AppBrowserController::OnReceivedInitialURL()`**
    *   **Location:**
        `chrome/browser/ui/web_applications/app_browser_controller.cc`
    *   **Trigger:** Fires once when the app window is first opened and the
        initial navigation completes.
    *   **Analysis:** This sets the initial state. While a manifest update could
        theoretically happen immediately after this, the incorrect state would
        be temporary and corrected by the next navigation via
        `OnTabStripModelChanged`. The risk is negligible.

3.  **`WebAppBrowserController::OnRelationshipCheckComplete(...)` (ChromeOS
    only)**
    *   **Location:**
        `chrome/browser/ui/web_applications/web_app_browser_controller.cc`
    *   **Trigger:** An asynchronous callback that fires after a Digital Asset
        Link check for a TWA completes.
    *   **Analysis:** As an asynchronous callback, this could race with a
        manifest update. However, the potential for an incorrect state is brief
        and would be corrected by the next user navigation. The risk is low.

This exhaustive analysis confirms that the `OnTabStripModelChanged` call
provides a robust mechanism to prevent any persistent incorrect UI state.

### Step 7 (2025-08-20): Test Analysis

An investigation into the existing test suite was conducted to identify how web
app scope and extended scope are tested.

*   **Direct Scope Modification in Unit Tests:** Direct calls to
    `WebApp::SetScope` were found in
    `chrome/browser/web_applications/web_app_unittest.cc` and
    `chrome/browser/web_applications/web_app_registrar_unittest.cc`. These are
    unit tests for the `WebApp` and `WebAppRegistrar` classes, respectively.
    They verify internal logic by directly manipulating in-memory objects and do
    not involve UI interaction, making them unsuitable for testing the custom
    tab bar's visibility.

*   **Scope Modification in Browser Tests:** Browser tests, which are necessary
    for UI testing, do not call `SetScope` directly. Instead, they install web
    apps from manifests with specific `scope` and `scope_extensions` to trigger
    the desired behavior.

*   **`WebAppScopeExtensionsBrowserTest`:** The most relevant test suite was
    found in
    `chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`.
    This suite specifically tests the behavior of `scope_extensions`, including
    link capturing and how the effective scope is determined by the
    `validated_scope_extensions`.

*   **Dynamic Scope Changes:** The
    `WebAppScopeExtensionsOriginTrialBrowserTest.OriginTrial` test is
    particularly relevant as it demonstrates an app's scope changing *after*
    installation. It installs an app with an origin trial token that enables
    scope extensions, and then reloads the app without the token, triggering a
    manifest update that removes the extended scope.

This analysis confirms that `WebAppScopeExtensionsBrowserTest` is the ideal
place to add tests for the dynamic toolbar visibility feature, as it already
covers the installation and update scenarios that lead to changes in
`validated_scope_extensions` and follows the correct pattern for UI testing.

### Step 8 (2025-08-20): Comprehensive `UpdateCustomTabBarVisibility` Caller Analysis

A comprehensive search for all callers of `UpdateCustomTabBarVisibility` was
performed to ensure no scenarios were missed. The following call sites were
identified:

1.  **`AppBrowserController::OnTabStripModelChanged(...)`**
    *   **Location:**
        `chrome/browser/ui/web_applications/app_browser_controller.cc`
    *   **Trigger:** Fires after any navigation or tab change is complete.
    *   **Analysis:** This is the most critical call site. It functions as a
        failsafe, as it will always execute at the end of a navigation. This
        ensures that it will correct the toolbar's visibility based on the
        final, committed URL and the most current app scope.

2.  **`AppBrowserController::OnReceivedInitialURL()`**
    *   **Location:**
        `chrome/browser/ui/web_applications/app_browser_controller.cc`
    *   **Trigger:** Fires once when the app window is first opened and the
        initial navigation completes.
    *   **Analysis:** This sets the initial state of the toolbar.

3.  **`Browser::ActiveTabChanged(...)`**
    *   **Location:** `chrome/browser/ui/browser.cc`
    *   **Trigger:** Fires when the active tab in the browser changes.
    *   **Analysis:** This method is responsible for synchronizing the browser's
        UI with the state of the newly activated tab. It makes two calls to
        `UpdateCustomTabBarVisibility`. The first call, with `animate=false`,
        ensures a rapid and non-animated update of the toolbar's visibility to
        match the new tab's content. A second call, with `animate=true`, is made
        after checking for a "sad tab" (crashed tab) state, suggesting that
        visibility changes related to the tab's health should be animated. This
        robustly handles visibility updates during normal tab switching.

4.  **`WebAppBrowserController::OnRelationshipCheckComplete(...)` (ChromeOS
    only)**
    *   **Location:**
        `chrome/browser/ui/web_applications/web_app_browser_controller.cc`
    *   **Trigger:** An asynchronous callback that fires after a Digital Asset
        Link check for a Trusted Web Activity (TWA) completes.
    *   **Analysis:** This updates the toolbar based on the verification status
        of the TWA.

5.  **Test-only Callers:**
    *   **Locations:**
        `chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_browsertest.cc`,
        `chrome/browser/ui/web_applications/web_app_browsertest.cc`
    *   **Trigger:** Called directly within test bodies.
    *   **Analysis:** These are used to set up specific UI states for testing
        purposes or to disable animations, and do not affect production
        behavior.

This more exhaustive analysis confirms that the visibility of the custom tab bar
is robustly managed during all major navigation and tab lifecycle events,
reinforcing the conclusion that the proposed change is safe.

### Step 9 (2025-08-20): `WebContentsDelegate` Method Analysis

An analysis of the key `WebContentsDelegate` methods involved in updating the UI
was performed to understand their triggers.

*   **`NavigationStateChanged`**:
    *   **Triggered by:** This method is called from `WebContentsImpl` in
        response to a wide variety of navigation and state changes, including
        URL changes, load state changes, title changes, and audio state changes.
    *   **Analysis:** This is a general-purpose notification that the state of
        the `WebContents` has changed in some way that might require a UI
        update. In `Browser::NavigationStateChanged`, it calls
        `ScheduleUIUpdate`, which asynchronously updates the UI.

*   **`VisibleSecurityStateChanged`**:
    *   **Triggered by:** This method is called from `WebContentsImpl` whenever
        the visible security state of the `WebContents` changes.
    *   **Analysis:** This is a specific notification that the security
        indicator in the omnibox and other security-related UI should be
        updated. In `Browser::VisibleSecurityStateChanged`, it calls
        `UpdateToolbarSecurityState`, which synchronously updates the toolbar.
        It also calls `UpdateCustomTabBarVisibility` to ensure the custom tab
        bar reflects the current security state.

This analysis confirms that the existing triggers for
`UpdateCustomTabBarVisibility` are tied to navigation, tab switching, and
security state changes, all of which are appropriate times to re-evaluate the
visibility of the custom tab bar.

### Step 10 (2025-08-20): Security State Change Analysis

To understand the root triggers for `VisibleSecurityStateChanged`, an
investigation into its callers was conducted.

*   **`SSLManager`:** The primary caller is the `SSLManager`. After a navigation
    completes, the `SSLManager` processes the security information for the new
    page (e.g., the SSL certificate) and updates the `NavigationEntry`. It then
    calls `WebContentsImpl::DidChangeVisibleSecurityState`, which in turn calls
    the delegate's `VisibleSecurityStateChanged` method.
*   **Other Triggers:** The security state can also change due to other events,
    such as the detection of mixed content on a page or when a user chooses to
    proceed through an SSL warning.

This confirms that security state changes are closely tied to navigation events
and the dynamic loading of content on a page.

### Step 11 (2025-08-20): Scope Checking and UI Update Timing Analysis

A detailed analysis was performed to identify the exact location of the scope
check and to understand the timing of the UI update relative to navigation
events.

*   **Scope Checking Logic:** The core logic for determining whether the custom
    tab bar should be shown resides in
    `AppBrowserController::ShouldShowCustomTabBar()`. This method performs the
    following checks on the active `WebContents`:
    1.  It checks if the current URL is within the app's scope by calling
        `IsUrlInAppScope()`.
    2.  It checks if the URL's scheme is secure (e.g., HTTPS or a secure origin
        like `localhost`).
    3.  It checks the overall security of the content using
    `webapps::InstallableEvaluator::IsContentSecure()`. If any of these checks
    indicate that the content is out of scope or insecure, the method returns
    `true`, signaling that the custom tab bar should be visible.

*   **UI Update Timing and Call Chain:** The UI update is triggered as follows:
    1.  A navigation completes, or the active tab is changed.
    2.  The `TabStripModel` is notified of the change.
    3.  This triggers the `AppBrowserController::OnTabStripModelChanged`
        observer method.
    4.  This method *synchronously* calls `UpdateCustomTabBarVisibility()`.
    5.  `UpdateCustomTabBarVisibility()` then calls `ShouldShowCustomTabBar()`
        to get the definitive visibility state and updates the browser window.

*   **Relation to `NavigationStateChanged`:** The
    `WebContentsDelegate::NavigationStateChanged` method is called earlier in
    the navigation process. It triggers a more general, *asynchronous* UI update
    via `ScheduleUIUpdate`. The critical, synchronous update that ensures the
    final, correct state of the toolbar is the one triggered by
    `OnTabStripModelChanged` after the navigation is complete.

This analysis confirms that the scope check is performed reliably at the end of
a navigation, ensuring the UI reflects the correct state for the committed page.

### Step 12 (2025-08-20): Manifest Update Test Analysis

An analysis of the existing manifest update tests was conducted to identify
patterns for triggering and verifying manifest updates.

*   **`ManifestUpdateManagerBrowserTest`**: This test fixture, located in
    `chrome/browser/web_applications/manifest_update_manager_browsertest.cc`, is
    the primary browser test for the manifest update process. It uses an
    `EmbeddedTestServer` and a request handler override (`OverrideManifest`) to
    serve different versions of a manifest. This allows the tests to simulate a
    manifest update by navigating to the app's URL after changing the manifest
    content. The `UpdateCheckResultAwaiter` utility is used to wait for the
    update process to complete.

*   **`ManifestUpdateCheckCommandTest` and
    `ManifestUpdateFinalizeCommandTest`**: These unit tests, located in
    `chrome/browser/web_applications/commands/`, test the
    `ManifestUpdateCheckCommand` and `ManifestUpdateFinalizeCommand` in
    isolation. They do not trigger a full manifest update, but rather test the
    logic of the individual commands.

*   **`UpdateAwaiter`**: The `UpdateAwaiter` class in
    `chrome/browser/ui/web_applications/test/web_app_browsertest_util.h` is a
    test utility designed to wait for the `OnWebAppManifestUpdated` event. This
    provides a reliable mechanism for writing browser tests that trigger and
    wait for a manifest update to complete.

This analysis confirms that there is a robust set of tools and patterns for
testing manifest updates, both at the unit and browser test level.

### Step 13 (2025-08-20): `ManifestSilentUpdateCommand` Test Analysis

An analysis of
`chrome/browser/web_applications/commands/manifest_silent_update_command_unittest.cc`
was conducted to understand the specifics of how manifest updates are processed.

*   **Purpose:** This test file covers the `ManifestSilentUpdateCommand`, which
    is responsible for checking for and applying manifest changes that do not
    require user permission.
*   **Update Classification:** The tests reveal a clear distinction between
    "silent" (non-security) updates and "security" updates.
    *   **Silent Updates:** Changes to fields like `scope`, `scope_extensions`,
        `start_url`, `theme_color`, `display_mode`, etc., are considered silent.
        These changes are applied to the `WebApp` object immediately. The tests
        `ScopeUpdatedSilently` and `ScopeExtensionsUpdatedSilently` explicitly
        confirm this behavior.
    *   **Security Updates:** Changes to `name` or significant changes to
        `icons` are considered security-sensitive. These changes are not applied
        immediately but are stored in a `pending_update_info` field on the
        `WebApp` object. They are applied later, typically upon user launch, to
        ensure the user is aware of the change.
*   **Conclusion:** This analysis provides strong evidence that changes to an
    app's scope are applied directly to the `WebApp` object in the
    `WebAppRegistrar` as part of the manifest update process. This reinforces
    the conclusion that the `OnWebAppManifestUpdated` observer notification is
    the correct and reliable trigger to use for re-evaluating the custom tab
    bar's visibility, as it fires after these scope changes have been committed.

### Step 14 (2025-08-20): Sync Flow Analysis

An investigation into the web app sync flow was conducted to ensure scope
changes from sync would be handled.

*   **`WebAppSyncBridge`:** The `WebAppSyncBridge` is the core component for
    handling incoming sync data. In methods like `MergeFullSyncData` and
    `ApplyIncrementalSyncChanges`, it directly modifies `WebApp` objects in the
    registrar via a `ScopedRegistryUpdate`.
      - Note from future: HOWEVER - changes in the sync scope are not then used
        to write to the WebApp object.
*   **No `WebAppInstallFinalizer`:** Crucially, this flow does *not* involve the
    `WebAppInstallFinalizer`. This means that neither `OnWebAppInstalled` nor
    `OnWebAppManifestUpdated` are called for sync-driven changes.
*   **`OnWebAppsWillBeUpdatedFromSync`:** The `WebAppSyncBridge` *does* call the
    `WebAppRegistrarObserver::OnWebAppsWillBeUpdatedFromSync` method before
    committing changes. This provides a reliable hook to observe scope changes
    originating from sync.
*   **Conclusion:** This analysis revealed that relying solely on
    `WebAppInstallManagerObserver` is insufficient. A complete solution must
    also observe the `WebAppRegistrar` to handle sync-based updates.

### Step 15 (2025-08-21): Analysis of Browser Test Setup for Preinstalled Apps

To properly test re-installation scenarios that mimic default or OEM apps, an
investigation into the `PreinstalledWebAppManager` and `WebAppProvider` startup
was conducted.

*   **`WebAppProvider` Startup:** The `WebAppProviderFactory` is responsible for
    creating the `WebAppProvider` for a profile. In its
    `BuildServiceInstanceForBrowserContext` method, it instantiates the provider
    and immediately calls `provider->Start()`. This synchronous start makes it
    difficult for browser tests to intervene and modify subsystems before they
    begin their startup tasks.

*   **`PreinstalledWebAppManager` Startup:** The `PreinstalledWebAppManager`'s
    `Start()` method initiates a process to load app configurations from JSON
    files on disk. This process is asynchronous.

*   **Existing Test Hooks:**
    *   **`FakeWebAppProvider`:** In unit tests, the `FakeWebAppProvider` is
        used. It provides direct control over subsystem creation and startup,
        allowing tests to replace components like `PreinstalledWebAppManager`
        entirely. This is not suitable for integration-style browser tests that
        aim to use the real provider.
        *   **`FakeWebAppProviderCreator`** Is a way that browser_tests can use
            a `FakeWebAppProvider`, as by default it uses the normal one.
    *   **`PreinstalledWebAppManager::SetConfigsForTesting`:** A global static
        `g_configs_for_testing` exists, which allows tests to inject a list of
        `base::Value` configs, bypassing the file-loading process. While
        functional, this relies on a global variable and requires the test to
        construct the JSON `Value` representation rather than the more direct
        `ExternalInstallOptions` struct.

*   **Identified Gap:** There is no existing mechanism for a browser test to use
    the *real* `WebAppProvider` but delay its startup to configure one of its
    subsystems (like `PreinstalledWebAppManager`) with test-specific data.
    Simulating a preinstalled app update requires this level of control to
    inject a new "preinstalled" configuration before the manager synchronizes
    its apps. The current testing approach for this scenario would have to rely
    on the `ExternallyManagedAppManager`. Investigation reveals this manager is
    a general-purpose subsystem responsible for handling all non-user, non-sync
    installations (e.g., from enterprise policy, preinstalled apps, and system
    apps), making it a viable but less specific alternative to testing the
    preinstalled flow directly.

## Summary of Findings

### The Problem
The custom tab bar in a web app window does not update its visibility when the
app's scope is dynamically changed while the app is running.

### How Scope Changes
An app's scope is modified through three primary mechanisms, all of which are
eventually orchestrated by the `WebAppInstallFinalizer` or the
`WebAppSyncBridge`:

1.  **Manifest Update:** The `ManifestSilentUpdateCommand` process handles
    manifest changes that do not require user permission. The investigation into
    this command (Step 13) confirms that changes to `scope` and
    `scope_extensions` are considered "silent" (non-security) updates. These
    changes are applied directly and synchronously to the `WebApp` object in the
    `WebAppRegistrar` before the `OnWebAppManifestUpdated` observer is notified.

2.  **Re-installation:** An existing app can be re-installed (e.g., by
    enterprise policy) via `WebAppInstallFinalizer::FinalizeInstall`. This also
    updates the scope and triggers the `OnWebAppInstalled` observer
    notification.

3.  **Sync:** The `WebAppSyncBridge` applies changes from sync directly to the
    `WebApp` objects in the registrar. This process does not use the
    `WebAppInstallFinalizer` and instead notifies observers via
    `OnWebAppsWillBeUpdatedFromSync`.
      - Note from future: Changes in the sync scope are not then used to write
        to the WebApp object. Only the user_display_mode is synced if the app is
        already installed.

### Current UI Update Mechanism
The visibility of the custom tab bar is determined by
`AppBrowserController::ShouldShowCustomTabBar()`, which checks if the current
URL is within the app's scope. This check is triggered by
`AppBrowserController::UpdateCustomTabBarVisibility()`. The primary callers of
`UpdateCustomTabBarVisibility` are:
*   `AppBrowserController::OnTabStripModelChanged`: Fires synchronously after a
    navigation or tab change is complete. This is the most critical failsafe
    trigger.
*   `Browser::ActiveTabChanged`: Fires when the active tab changes.
*   `Browser::VisibleSecurityStateChanged`: Fires when the security state of the
    page changes, which is typically triggered by the `SSLManager` after a
    navigation.
*   `AppBrowserController::OnReceivedInitialURL`: Fires once when the app is
    first opened.

### Navigation and UI Update Flow
1.  During a navigation, `WebContentsDelegate::NavigationStateChanged` is
    called, which triggers a general, asynchronous UI update.
2.  After the navigation is complete and committed,
    `AppBrowserController::OnTabStripModelChanged` is called.
3.  This triggers a synchronous call to `UpdateCustomTabBarVisibility`, which
    executes the scope check and updates the UI to its final, correct state.

### The Gap
There is no existing mechanism that triggers a UI update in response to a change
in the app's underlying scope definition. The UI is only updated in response to
navigation, tab, or security state changes.

### Existing Testing Infrastructure
The following test suites and fixtures are relevant for testing scope-related
behaviors and web app updates:

*   **Direct Scope Manipulation (Unit Tests):**
    *   **`WebAppTest`:** Used in `web_app_unittest.cc`, this fixture allows for
        the creation of `WebApp` objects and setting their scope via
        `WebAppInstallInfo` before installation.
    *   **`WebAppRegistrarTest`:** Used in `web_app_registrar_unittest.cc`, this
        fixture allows for direct manipulation of `WebApp` objects in the
        registrar, including calling `WebApp::SetScope`.

*   **Web App Updates (Unit Tests):**
    *   **`WebAppInstallFinalizerUnitTest`:** Used in
        `web_app_install_finalizer_unittest.cc`, this fixture directly calls
        `WebAppInstallFinalizer::FinalizeUpdate`, providing a way to test the
        update process in isolation.
    *   **`ShortcutSubManagerUnitTest`:** Used in
        `os_integration/shortcut_sub_manager_unittest.cc`, this fixture also
        triggers updates via `FinalizeUpdate` to test OS integration aspects.
    *   **`IsolatedWebAppApplyUpdateCommandUnitTest`:** Used in
        `isolated_web_apps/commands/isolated_web_app_apply_update_command_unittest.cc`,
        this fixture also triggers updates via `FinalizeUpdate` to test IWAs.
    *   **`ManifestUpdateCheckCommandTest` and
        `ManifestUpdateFinalizeCommandTest`**: These unit tests, located in
        `chrome/browser/web_applications/commands/`, test the manifest update
        commands in isolation.

*   **Scope and `scope_extensions` (Browser Tests):**
    *   **`WebAppScopeExtensionsBrowserTest`**: Located in
        `chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`,
        this suite is ideal for testing behaviors related to `scope_extensions`.
        The `WebAppScopeExtensionsOriginTrialBrowserTest.OriginTrial` test is
        particularly useful as it provides a pattern for triggering a manifest
        update that dynamically changes an app's scope after installation.

*   **General Web App Installation (Browser Tests):**
    *   Other browser tests, such as those in
        `chrome/browser/ui/web_applications/web_app_browsertest.cc`, install web
        apps from manifests and can be adapted to test changes to the `scope`
        attribute.

*   **Manifest Update Testing (Browser Tests):** As analyzed in Step 12, a
    robust infrastructure for testing manifest updates exists. The
    `ManifestUpdateManagerBrowserTest` provides the primary test fixture, using
    an `EmbeddedTestServer` to serve different manifest versions and simulate an
    update. The `UpdateAwaiter` utility in `web_app_browsertest_util.h` offers a
    reliable way to wait for the `OnWebAppManifestUpdated` event, which is
    critical for verifying the results of a dynamic update.

## Idea Explorations

### Requirements for a Solution
A solution must trigger a re-evaluation of the custom tab bar's visibility
whenever an app's scope or validated scope extensions are modified for a running
app.

### Idea 1: Observe `WebAppRegistrar` for Scope Changes
*   **Implementation:** Add a new method to `WebAppRegistrarObserver`, such as
    `OnWebAppScopeChanged(const AppId& app_id)`. The `WebAppBrowserController`
    would implement this observer method and call
    `UpdateCustomTabBarVisibility()` in response.
*   **Pros:** This is a clean, decoupled approach. It observes the source of
    truth for app data (`WebAppRegistrar`) and directly notifies interested
    parties of the relevant change.
*   **Cons:** Requires adding a new method to a widely used observer interface.

### Idea 2: Leverage the Existing `WebAppInstallManagerObserver`
*   **Implementation:** Add a call to `UpdateCustomTabBarVisibility()` within
    the existing `WebAppBrowserController::OnWebAppManifestUpdated()` method.
*   **Pros:** This is a minimally invasive change, as the
    `WebAppBrowserController` already observes for manifest updates. The
    investigation into `ManifestSilentUpdateCommand` (Step 13) provides strong
    evidence for this approach, confirming that scope changes are applied
    synchronously during the update process *before* `OnWebAppManifestUpdated`
    is called. This makes it a reliable and correctly timed trigger.
*   **Cons:** This approach is less direct. It relies on the fact that scope
    changes currently happen during manifest updates. If a future change allowed
    scope to be modified outside of the manifest update process, this solution
    would no longer work.

### Risk Analysis

A key risk is a race condition where a scope update occurs mid-navigation. This
is mitigated by the existing `AppBrowserController::OnTabStripModelChanged`
method, which acts as a failsafe by always re-evaluating and correcting the
toolbar's visibility at the end of any navigation, ensuring any transient
incorrect state is corrected.

### Testing Plan

This section outlines the testing strategy for verifying the dynamic update of
the custom tab bar's visibility. It includes a list of relevant test fixtures
and specific test cases for unit, browser, and manual testing.

### Relevant Test Fixtures

*   **Unit Test Fixtures:**
    *   `WebAppBrowserControllerUnitTest` in
        `chrome/browser/ui/web_applications/web_app_browser_controller_unittest.cc`:
        Ideal for testing the `WebAppBrowserController` in isolation, allowing
        for mocking of the `BrowserWindow` to verify that
        `UpdateCustomTabBarVisibility` is called in response to the correct
        event.
    *   `ManifestUpdateCheckCommandTest` and `ManifestUpdateFinalizeCommandTest`
        in `chrome/browser/web_applications/commands/`: Useful for verifying the
        logic of the manifest update commands themselves, ensuring that scope
        changes are correctly identified and applied.

*   **Browser Test Fixtures:**
    *   `WebAppScopeExtensionsBrowserTest` in
        `chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`:
        The most suitable fixture for this feature. It is specifically designed
        for testing `scope_extensions` and includes tests that dynamically
        change an app's scope after installation (e.g.,
        `WebAppScopeExtensionsOriginTrialBrowserTest.OriginTrial`).
    *   `ManifestUpdateManagerBrowserTest` in
        `chrome/browser/web_applications/manifest_update_manager_browsertest.cc`:
        The primary fixture for testing the end-to-end manifest update process.
        It provides the necessary infrastructure, such as an
        `EmbeddedTestServer` and the `UpdateAwaiter` utility, to simulate and
        verify manifest updates in a browser environment.
    *   `WebAppBrowserTest` in
        `chrome/browser/ui/web_applications/web_app_browsertest.cc`: A
        general-purpose fixture for testing web app UI and behavior that can be
        adapted for testing changes to the base `scope` attribute.


## Meta

### Purpose of an Investigation Document

An investigation document is a record of the research and exploration of a
specific problem or area of the codebase. It gathers facts, and does not make
explicit proposals design decisions.

## Relevant Context

Maintain a list of important files, code search queries, and other resources
that were critical during the investigation. This serves as a quick reference
for anyone working on this system in the future.

*   **Files:**
    *   `//chrome/browser/ui/browser.cc`
    *   `//chrome/browser/ui/web_applications/app_browser_controller.h`
    *   `//chrome/browser/ui/web_applications/app_browser_controller.cc`
    *   `//chrome/browser/ui/web_applications/web_app_browser_controller.h`
    *   `//chrome/browser/ui/web_applications/web_app_browser_controller.cc`
    *   `//chrome/browser/ui/web_applications/test/web_app_browsertest_util.h`
    *   `//chrome/browser/web_applications/commands/manifest_silent_update_command_unittest.cc`
    *   `//chrome/browser/web_applications/manifest_update_manager_browsertest.cc`
    *   `//chrome/browser/web_applications/web_app.h`
    *   `//chrome/browser/web_applications/web_app_install_finalizer.h`
    *   `//chrome/browser/web_applications/web_app_install_finalizer.cc`
    *   `//chrome/browser/web_applications/web_app_install_utils.h`
    *   `//chrome/browser/web_applications/web_app_install_utils.cc`
    *   `//chrome/browser/web_applications/web_app_registrar.h`
    *   `//chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`
    *   `//chrome/browser/web_applications/web_app_tab_helper.h`
    *   `//chrome/browser/web_applications/web_app_tab_helper.cc`
    *   `//chrome/browser/sync/test/integration/single_client_web_apps_sync_test.cc`
    *   `//chrome/browser/web_applications/web_app_sync_bridge.h`
    *   `//chrome/browser/web_applications/web_app_sync_bridge.cc`
*   **Key Functions:**
    *   `AppBrowserController::UpdateCustomTabBarVisibility`
    *   `AppBrowserController::ShouldShowCustomTabBar`
    *   `WebAppBrowserController::OnWebAppManifestUpdated`
    *   `WebApp::SetScope`
    *   `WebApp::SetValidatedScopeExtensions`
    *   `WebAppInstallFinalizer::FinalizeInstall`
    *   `WebAppInstallFinalizer::FinalizeUpdate`

## Document History

| Date        | Summary of changes                                             |
| ------------------ | ------------------------------------------------------- |
| 2025-08-20 | Created the investigation document.       |
| 2025-08-20 | Added initial findings, production call site analysis, call chain analysis, `WebAppTabHelper` analysis, `WebAppBrowserController` analysis, and a comprehensive call site analysis for `UpdateCustomTabBarVisibility`. |
| 2025-08-20 | Added test analysis and updated production call site analysis with verified search results. |
| 2025-08-20 | Corrected and expanded test analysis based on a more complete set of test files. |
| 2025-08-20 | Restored and expanded Step 6 with a detailed analysis of `UpdateCustomTabBarVisibility` call sites. |
| 2025-08-20 | Updated conclusions with detailed implementation and testing plan. |
