# Design Doc: Dynamic Custom Tab Bar Visibility on Web App Scope Change

- **Author(s):** dmurph@chromium.org
- **Team:** PWA Team
- **Status:** In Review
- **Tracking Bug:** https://crbug.com/434956941
- **PRD:** https://bit.ly/predictable-webapp-updating-prd
- **Investigation Doc:**
  [scope_toolbar_dynamic_updating_investigation.md](scope_toolbar_dynamic_updating_investigation.md)
- **Planning Doc:** None

This document is for developer-AI collaboration. It is not meant to be reviewed
by a code reviewer, it is too verbose and not as helpful. I can be referenced if
that is helpful.

## Context

### Objective
To ensure that the custom tab bar in a web app window dynamically shows or hides
itself when the app's scope or scope extensions are modified.

### Background
The custom tab bar in a web app window serves as a visual indicator that the
user has navigated outside of the app's defined scope. Its visibility is
controlled by `AppBrowserController::UpdateCustomTabBarVisibility`. The
investigation revealed that this method is currently only called in response to
navigation and tab events.

A web app's effective scope (including `scope` and `scope_extensions`) can
change during its lifecycle, primarily through:
1.  **Installation/Update:** The `WebAppInstallFinalizer` processes new manifest
    information, which may contain a new scope.
2.  **Sync/Startup:** The `WebAppDatabaseSerialization` process rehydrates the
    `WebApp` from the database, which may contain an updated scope from another
    device.

Currently, there is no mechanism to propagate these scope changes to an open app
window, leading to a stale UI where the tab bar's visibility does not reflect
the app's current scope.

### Platforms
This design affects all desktop platforms where web apps are supported: Mac,
Windows, Linux, and Chrome OS.

## Detailed Design

### Overview
The proposed solution is to introduce a new `WebAppScope` class to encapsulate
scope information and add a new method to `WebAppRegistrarObserver` that passes
this object to observers. `WebAppBrowserController` will then use this object to
perform the scope check.

### Code Affected
*   `chrome/browser/ui/web_applications/web_app_browser_controller.cc`
*   `chrome/browser/ui/web_applications/web_app_browser_controller.h`
*   `chrome/browser/web_applications/web_app_scope.h` (new file)
*   `chrome/browser/web_applications/web_app_scope.cc` (new file)
*   `chrome/browser/web_applications/web_app_registrar.h`
*   `chrome/browser/web_applications/web_app_registrar.cc`
*   `chrome/browser/web_applications/web_app_install_finalizer.cc`
*   `chrome/browser/web_applications/web_app_sync_bridge.cc`

### Detailed Design
The solution involves introducing a new `WebAppScope` class to encapsulate scope
information and adding a new method to `WebAppRegistrarObserver` that passes
this object to observers.

1.  **New `WebAppScope` Class:**
    *   A new class, `WebAppScope`, will be created to hold all information
        related to an app's effective scope.
    *   It will store the app's `scope` GURL and its
        `validated_scope_extensions`.
    *   It will have a public method, `bool IsInScope(const GURL& url) const`,
        which will contain the logic to determine if a given URL is within the
        app's effective scope.

2.  **New `WebAppRegistrar::GetEffectiveScope` Method:**
    *   A new method will be added to the `WebAppRegistrar` to construct a
        `std::optional<WebAppScope>` object from an `app_id`.

3.  **New `WebAppRegistrarObserver::OnWebAppEffectiveScopeChanged` Method:**
    *   A new virtual method `OnWebAppEffectiveScopeChanged(const AppId& app_id,
        const WebAppScope& new_scope)` will be added to the
        `WebAppRegistrarObserver` interface.
    *   This method will be called from the `WebAppInstallFinalizer` after a
        successful update that modifies the scope.

4.  **`WebAppBrowserController` Implementation:**
    *   The `WebAppBrowserController` class will inherit from
        `WebAppRegistrarObserver`.
    *   It will register itself as an observer of the `WebAppRegistrar` for its
        profile.
    *   It will implement `OnWebAppEffectiveScopeChanged`. If the `app_id` in
        the notification matches its own `app_id`, it will use the provided
        `WebAppScope` object to determine if the current URL is in or out of
        scope and call `UpdateCustomTabBarVisibility` accordingly.

5.  **Notify scope observers:**
    *   The `OnWebAppEffectiveScopeChanged` observer method will be called from
        the three locations where an app's scope can be updated:
        `WebAppInstallFinalizer` (for installations/updates) and through the
        manifest update command which also funnels through
        `WebAppInstallFinalizer`.
    *   **`WebAppInstallFinalizer`:** In `FinalizeInstall` (for updates over
        existing apps) and `FinalizeUpdate`, a check will be added to compare
        the scope and `validated_scope_extensions` of the existing app with the
        new ones from the `WebAppInstallInfo`. A boolean `scope_changed` will be
        passed to `OnDatabaseCommitCompletedForInstall` and
        `OnDatabaseCommitCompletedForUpdate`. If the scope has changed and the
        database commit is successful,
        `registrar->NotifyWebAppEffectiveScopeChanged(app_id)` will be called.
    *   **`ManifestSilentUpdateCommand`:** This command uses
        `WebAppInstallFinalizer::FinalizeUpdate` to apply changes. Therefore,
        the changes in `WebAppInstallFinalizer` will cover manifest updates
        automatically.

*   Pros (other than solving the goal):
    *   The `WebAppScope` approach ensures that scope checks are efficient as
        the `WebAppBrowserController` no longer needs to re-query the registrar
        after a scope change notification. This is a is a good future-compatible
        concept / object.
    *   Having a scope-specific observer de-couples this change from when the
        change happens, which is not tightly coupling the toolbar process to the
        lifecycle of apps, which can change (and there may be more ways for
        scope to update in the future).


#### Automated Testing
The automated testing plan will be expanded to cover all relevant scenarios in
existing and new browser test suites. The core idea is to trigger a scope change
and then wait for the `OnWebAppEffectiveScopeChanged` notification before
verifying the toolbar's visibility.

To achieve a realistic test for re-installation, a new test hook will be added:
*   **`PreinstalledWebAppManager::SetParsedConfigsForTesting()`:** A new static
    method to inject `ExternalInstallOptions` directly, bypassing file loading
    and parsing. This will use a `base::AutoReset` to manage the test-only
    configuration, ensuring it's automatically cleaned up after the test.

1.  **New Test File (`web_app_browser_controller_browsertest.cc`):** A new
    browser test file will be created to house tests for the controller's UI
    behavior.
    *   **Re-installation Test:** This test will simulate a user-initiated
        installation over an existing preinstalled app. In the test's
        constructor or `SetUp`, it will call `SetParsedConfigsForTesting` to
        configure the `PreinstalledWebAppManager` to install an app with a
        narrow scope on startup. The test will then launch the app, navigate to
        a URL that is out of scope, and verify that the custom tab bar is
        visible. After that, it will trigger a user-initiated installation of
        the same app but with a new manifest that has a wider scope (covering
        the current URL). The test will wait for the
        `OnWebAppEffectiveScopeChanged` notification and
        verify the toolbar becomes hidden.

2.  **Sync Update Test (in `single_client_web_apps_sync_test.cc`):** A new test
    will be added to this existing suite to verify UI updates from sync.
    *   It will install a web app and open it in a window.
    *   It will navigate the app window to a URL that is initially out of scope
        and verify the custom tab bar is visible.
    *   It will then get the app's entity from the `FakeServer`, modify its
        `scope` field in the `WebAppSpecifics` proto, and commit the change back
        to the server.
    *   This will trigger a sync update to the client. The test will wait for
        the `OnWebAppEffectiveScopeChanged` notification.
    *   Finally, it will verify that the custom tab bar is now hidden.

3.  **New Test File (`manifest_silent_update_command_browsertest.cc`):** A
    browser test version of the existing unit test will be created to test the
    modern manifest update flow.
    *   **Simple Scope Update Test:** This test will install an app with a
        narrow scope, open it in a window, and navigate out of scope (verifying
        the toolbar is visible). It will then trigger a
        `ManifestSilentUpdateCommand` with a new manifest that has a wider
        scope, wait for the `OnWebAppEffectiveScopeChanged` notification, and
        verify the toolbar becomes hidden after the update completes.

4.  **Scope Extension Update Test (in
    `web_app_scope_extensions_browsertest.cc`):** A new test will be added to
    this existing suite.
    *   It will install an app with `scope_extensions`, open it in a window, and
        navigate to a URL within the extended scope (verifying the toolbar is
        hidden).
    *   It will then trigger a manifest update that removes the
        `scope_extensions` (similar to the existing `OriginTrial` test).
    *   Finally, it will wait for the `OnWebAppEffectiveScopeChanged`
        notification and verify the custom tab bar is now visible.


## Alternatives Considered

### Alternative A: New `WebAppRegistrarObserver` Method without `WebAppScope` object
This approach is similar to the main proposal but simplifies the observer
interface.
*   **Implementation:** A new `OnWebAppEffectiveScopeChanged(const AppId&
    app_id)` method is added to `WebAppRegistrarObserver`. The
    `WebAppBrowserController` implements this, and upon notification, it calls
    `UpdateCustomTabBarVisibility()`. The existing `IsUrlInAppScope` method
    continues to query the `WebAppRegistrar` to get the latest scope
    information.
*   **Pros:** Simpler observer interface. Avoids creating and passing a new
    `WebAppScope` object.
*   **Cons:** The `WebAppBrowserController` must re-query the `WebAppRegistrar`
    for scope information upon every notification, which is slightly less
    efficient than having the new scope data pushed to it.

### Alternative B: Use Existing `WebAppRegistrarObserver` and `WebAppInstallManagerObserver` methods
This approach involves using the existing `OnWebAppWillBeUpdatedFromSync` and
`OnWebAppInstalled` observer methods to trigger the UI update.
*   **Pros:** Avoids adding a new method to the `WebAppRegistrarObserver`
    interface.
*   **Cons:** Less explicit. It couples the UI update to broader "install" and
    "sync" events rather than the specific data change we care about (scope
    modification). This is less maintainable, as a future change to the install
    or sync flows could inadvertently break the UI update logic if it no longer
    signals through these methods.

### Alternative C: Use `WebAppInstallManagerObserver` Only
The initial plan was to only use `WebAppInstallManagerObserver` methods
(`OnWebAppInstalled` and `OnWebAppManifestUpdated`).
*   **Pros:** Minimally invasive, as `WebAppBrowserController` already observes
    this.
*   **Cons:** Ties implementation to internal process.

### Alternative Test Strategy: Using `ExternallyManagedAppManager`
An alternative to the proposed testing strategy of integrating with the
`PreinstalledWebAppManager` is to use the `ExternallyManagedAppManager`
directly. This manager is a general-purpose subsystem responsible for handling
all non-user, non-sync installations (e.g., from enterprise policy, preinstalled
apps, and system apps). It provides a `SynchronizeInstalledApps` method which is
a useful high-level API for tests.

*   **Implementation:** Tests would call
    `ExternallyManagedAppManager::InstallNow()` or schedule a synchronization to
    trigger a re-installation of an app with a modified manifest.
*   **Pros:** Simpler to implement as it doesn't require new testing hooks to
    delay provider startup. It directly uses an existing public interface of a
    subsystem.
*   **Cons:** Less specific. While `PreinstalledWebAppManager` uses
    `ExternallyManagedAppManager` under the hood, testing via the latter skips
    the configuration loading and processing logic of the former. For a test
    that specifically aims to simulate a preinstalled app update, using the
    `PreinstalledWebAppManager`'s public interface is a more accurate and less
    brittle approach.

## Follow-up Work

### Formalize the `WebAppScope` Object
The `WebAppScope` object is currently only used for the
`OnWebAppEffectiveScopeChanged` observation. Future work could involve
integrating this object more deeply into the system for greater consistency and
to reduce redundant registrar queries.

*   **Refactor `WebAppBrowserController::IsUrlInAppScope`:** As identified
    during the investigation, this method duplicates logic from the
    `WebAppRegistrar`. It should be refactored to delegate the scope check to
    `WebAppRegistrar::IsUrlInAppExtendedScope`, while keeping its
    ChromeOS-specific logic. This would centralize the core scope-checking logic
    in the registrar.

*   **Cache `WebAppScope` in `WebAppBrowserController`:** After the above
    refactoring, the `WebAppBrowserController` could be further improved by
    holding a `WebAppScope` member object. This object would be initialized at
    construction and updated by the `OnWebAppEffectiveScopeChanged` observer.
    This would make the controller's scope-checking logic self-contained and
    avoid querying the registrar on every navigation.

### Code Health: De-duplicate and Centralize Scope Checking
The investigation revealed that the scope-checking logic in
`WebAppBrowserController::IsUrlInAppScope` is more correct than similar methods
in `WebAppRegistrar`, as it properly handles same-origin checks and
HTTP-to-HTTPS scheme upgrades as per the W3C manifest specification.

A future refactoring should move this correct logic from the controller into a
new, centralized method in the registrar,
`WebAppRegistrar::IsUrlInAppEffectiveScope(const GURL& url, const
webapps::AppId& app_id) const`. The `WebAppBrowserController::IsUrlInAppScope`
method would then be simplified to call this new registrar method, while
retaining its ChromeOS-specific checks.

## Technical Debt
This is a small, targeted change that fixes an inconsistency. It does not
introduce any new technical debt, nor does it address any significant existing
debt.

## Meta

### Purpose of a Design Doc
A design doc is a living document that serves as the single source of truth for
a project. Its primary purpose is to force clarity of thought, to build
consensus among stakeholders, and to serve as a historical record for future
developers. It should clearly articulate the problem, the proposed solution, and
the reasoning behind the chosen path, including why alternatives were discarded.
It should not be a living document (systems change), but instead documenting a
single point of time.

## Key Files and Context
*   `//chrome/browser/ui/web_applications/web_app_browser_controller.h`
*   `//chrome/browser/ui/web_applications/web_app_browser_controller.cc`
*   `//chrome/browser/ui/web_applications/app_browser_controller.cc`
*   `//chrome/browser/web_applications/web_app_install_finalizer.cc`
*   `//chrome/browser/web_applications/web_app_install_manager.h`
*   `//chrome/browser/web_applications/web_app_registrar.h`
*   `//chrome/browser/web_applications/externally_managed_app_manager.h`
*   `//chrome/browser/web_applications/commands/manifest_silent_update_command.h`
*   `//chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`
*   `//chrome/browser/sync/test/integration/single_client_web_apps_sync_test.cc`

## Document history

| Date        | Summary of changes                                                                      |
| - |  |
| 2025-08-20 | Created the design document. |
| 2025-08-20 | Added timing verification and refactored testing plan. |
| 2025-08-20 | Added all standard sections from the design doc template. |
| 2025-08-20 | Updated design to handle re-installations via `OnWebAppInstalled` and expanded testing plan. |
