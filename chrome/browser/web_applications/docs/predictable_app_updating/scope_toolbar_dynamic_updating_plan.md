# Project: Dynamic Toolbar Updating Implementation Plan

* **Investigation Doc:**
[scope_toolbar_dynamic_updating_investigation.md](scope_toolbar_dynamic_updating_investigation.md)
* **Design Doc:**
[scope_toolbar_dynamic_updating_design.md](scope_toolbar_dynamic_updating_design.md)

This document is for developer-AI collaboration. It is not meant to be reviewed
by a code reviewer, it is too verbose and not helpful. I can be referenced if
that is helpful.

## 1. Instructions for Self

My goal is to implement *Dynamic Toolbar Updating* by following the step-by-step plan
outlined below.

I will read all of these files in the "Key Files and Context" section along
with this plan.

-   **Source control choice:** git
-   **My build configuration/s for this project will be:**
    -    `out/Release`

**My process for each step will be:**

I will execute these steps *strictly* in order. This is the most important
context to keep as is.

1.  Start a new branch for the project if needed.
1.  Implement the code changes required for the task, adhering to Chromium
    coding standards and conventions. I will use the "Key Files and Context"
    section as a reference. I will code search definitions for any APIs I am
    using. I will make sure to include necessary header files. I will make

    sure to include class declarations for new members as needed. I will
    include new .h/.cc files in the appropriate BUILD.gn targets.
2.  After implementing the changes, I will compile the tests to verify the
    changes.
3.  If the compilation is successful, I will run targeted tests.
4.  If the tests pass, I will do **ALL** of the following:
    *   Mark the task as in review (`[x]`) in this file.
    *   Create a commit with a descriptive, multi-line message of
        the changes made, and motivation.
5.  If the compilation or tests fail and I cannot resolve the issue, I will, I
    will do **ALL** of the following:
    *   Mark the task as failed (`[! FAILED]` in this file).
    *   Add a note to the "Unresolved Issues" section detailing the problem.
    *   Wait for user input before proceeding.

I will update this document after each step to reflect the current status of
the project. I will add to the `Key Files and Context` section when I create
new files, or read important files.

## 2. Background, Motivation, and Requirements

This project aims to fix a UI bug where the custom tab bar in a web app window
does not update its visibility when the app's scope is changed through a
manifest update, re-installation, or sync event. The core requirement is to
ensure the toolbar's visibility is always consistent with the app's current
effective scope.

## 3. High-Level Plan

The implementation will be broken down into vertical slices, with each phase
delivering an end-to-end testable piece of functionality.
1.  **MVP for Manifest Updates:** Implement the core infrastructure and get the
    manifest update flow working end-to-end with a browser test.
2.  **Sync Update Vertical:** Add support for scope changes coming from Sync.
3.  **Re-installation Vertical:** Add support for scope changes from
    re-installations and build the necessary testing infrastructure.
4.  **Final Test Coverage:** Add remaining specific test cases.

## 4. Key Files and Context

*   `//chrome/browser/web_applications/docs/predictable_app_updating/scope_toolbar_dynamic_updating_design.md`
*   `//chrome/browser/web_applications/docs/predictable_app_updating/scope_toolbar_dynamic_updating_investigation.md`
*   `//chrome/browser/ui/web_applications/web_app_browser_controller.h`
*   `//chrome/browser/ui/web_applications/web_app_browser_controller.cc`
*   `//chrome/browser/web_applications/web_app_registrar.h`
*   `//chrome/browser/web_applications/web_app_registrar.cc`
*   `//chrome/browser/web_applications/web_app_install_finalizer.h`
*   `//chrome/browser/web_applications/web_app_install_finalizer.cc`
*   `//chrome/browser/web_applications/web_app_sync_bridge.h`
*   `//chrome/browser/web_applications/web_app_sync_bridge.cc`
*   `//chrome/browser/web_applications/web_app_provider.h`
*   `//chrome/browser/web_applications/web_app_provider.cc`
*   `//chrome/browser/web_applications/web_app_provider_factory.cc`
*   `//chrome/browser/web_applications/preinstalled_web_app_manager.h`
*   `//chrome/browser/web_applications/preinstalled_web_app_manager.cc`
*   `//chrome/browser/web_applications/test/fake_web_app_provider.h`
*   `//chrome/browser/web_applications/web_app_scope_extensions_browsertest.cc`

## 5. TODO Log

### Phase 1: MVP for Manifest Updates

*   [x] **Task 1.1:** Create `WebAppScope` class and observer methods.
    *   Create `web_app_scope.h` and `web_app_scope.cc` with the `WebAppScope`
        class containing `scope`, `validated_scope_extensions`, and the
        `IsInScope` method. Add to `BUILD.gn`.
    *   Add `GetEffectiveScope(app_id)` to `WebAppRegistrar`.
    *   Add `OnWebAppEffectiveScopeChanged` to `WebAppRegistrarObserver`.
    *   Add `NotifyWebAppEffectiveScopeChanged` to `WebAppRegistrar`.
    *   **Build target:** `unit_tests`
*   [x] **Task 1.2:** Plumb notification through `WebAppInstallFinalizer` for
    updates.
    *   In `WebAppInstallFinalizer::FinalizeUpdate`, detect if scope has
        changed.
    *   Pass this information to `OnDatabaseCommitCompletedForUpdate`.
    *   If scope changed and commit succeeded, call
        `NotifyWebAppEffectiveScopeChanged`.
    *   **Build target:** `unit_tests`
*   [x] **Task 1.3:** Implement UI response in `WebAppBrowserController`.
    *   Make `WebAppBrowserController` inherit from `WebAppRegistrarObserver`.
    *   Add a `base::ScopedObservation` to observe the `WebAppRegistrar`.
    *   Implement `OnWebAppRegistrarDestroyed` to reset the observation.
    *   Implement `OnWebAppEffectiveScopeChanged` to use the provided
        `WebAppScope` object to check if the current URL is in scope, and then
        call `UpdateCustomTabBarVisibility()`.
    *   **Build target:** `unit_tests`
*   [x] **Task 1.4:** Write manifest update browser test.
    *   Create `manifest_silent_update_command_browsertest.cc` in the
        `commands/` subdir.
    *   Add a test that installs an app, opens it, navigates out of scope,
        triggers a `ManifestSilentUpdateCommand` that widens the scope, and
        verifies the toolbar hides. This is done by:
        *   Creating a test pages in chrome/test/data/web_apps/scope_updating:
            *   One to install, which has a manifest with a narrow scope
            *   One to navigate to, where we trigger silent updating on after it
                loads, which should update the scope.
            *   One to be out of scope, which we can use to see the out-of-scope
                bar being on or off.
        *   Trigger silent update via the scheduler
            `ScheduleManifestSilentUpdate` from `WebAppProvider::GetForTest`'s
            scheduler.
        *   Enable the feature that is required to have the
            ManifestSilentUpdateCommand work.
    *   (unplanned) Hook up calling ScheduleManifestSilentUpdate in the
        ManifestUpdateManager::MaybeUpdate class.
    *   (unplanned) Fix the WebAppDataRetriever to be compatible with not being
        installable or not passing the installable check, as this can happen for
        things like navigations etc.
    *   (unplanned) Make it so the silent manifest update command handles page
        navigations correctly.
    *   **Build target:** `browser_tests`
    *   **Tests to run:** `browser_tests --gtest_filter=ManifestSilentUpdateCommandBrowserTest.*`

### Phase 2: Re-installation Vertical and Test Infra

*   [x] **Task 3.1:** Implement testing infrastructure.
    *   Implement a static `PreinstalledWebAppManager::SetParsedConfigsForTesting`
        method that uses a `base::AutoReset` to manage the test-only
        configuration.
    *   **Build target:** `browser_tests`
*   [x] **Task 3.2:** Plumb notification through `WebAppInstallFinalizer` for
    installs.
    *   In `WebAppInstallFinalizer::FinalizeInstall`, detect if scope has
        changed for an existing app.
    *   Pass this information to `OnDatabaseCommitCompletedForInstall`.
    *   If scope changed and commit succeeded, call
        `NotifyWebAppEffectiveScopeChanged`.
    *   **Build target:** `unit_tests`
*   [x] **Task 3.3:** Write re-installation browser test.
    *   Create `web_app_browser_controller_browsertest.cc`.
    *   Add the test as described in the design doc, using the new test infra
        to simulate a preinstalled app being updated by a user install.
    *   **Build target:** `browser_tests`
    *   **Tests to run:** `browser_tests --gtest_filter=WebAppBrowserControllerBrowserTest.*`


### Phase 3: Final Test Coverage

*   [x] **Task 4.1:** Add scope extension update browser test for registry
    observer.
    *   **Build target:** `browser_tests`
    *   **Tests to run:** `browser_tests
        --gtest_filter=WebAppScopeExtensionsBrowserTest.*`
*   [x] **Task 4.2:** Write re-installation browser test that adds the bar back.
    *   Add the test to WebAppBrowserControllerBrowserTest, which tests the
        opposite of what is already tested there for reinstall - the original
        install should be at the wider scope, and the reinstall should narrow
        the scope. the toolbar should disappear.
    *   **Build target:** `browser_tests`
    *   **Tests to run:** `browser_tests
        --gtest_filter=WebAppBrowserControllerBrowserTest.*`

### Unresolved Issues
*(Log of failed tasks and blocking issues)*
