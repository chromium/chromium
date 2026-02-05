# Refactor Web App Command Result Types: Implementation Plan & TODO Log

* **Investigation Doc:** `//chrome/browser/web_applications/docs/code_health/command_results_separate_file_investigation.md`

## 1. Instructions for Self

My goal is to implement the refactoring of web app command result types by following the step-by-step plan outlined below.

I will read all of these files in the "Key Files and Context" section along with this plan.

-   **Source control choice:** git
-   **My build configuration/s for this project will be:**
    -    `out/Default`

**My process for each step will be:**

I will execute these steps *strictly* in order. This is the most important
context to keep as is.

1.  Start a new branch for the project if needed. For JJ, start a change with
    `jj new`.
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

As detailed in the investigation document, many web application commands define their result types, callbacks, and stream operators within the main command header file. This forces clients that only need to know about the outcome of a command to include a heavyweight header with many transitive dependencies, negatively impacting build times and violating the principle of separating interface from implementation.

This project aims to refactor 18 identified commands to move these result-related declarations into separate, more lightweight `_result.h` header files. This will improve build performance and code organization.

## 3. High-Level Plan

The refactoring will be done on a per-command basis. For each command:

1.  Create a new `_result.h` file in the appropriate scheduler directory:
    *   `//chrome/browser/web_applications/scheduler/` for general commands.
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/` for isolated web app commands.
    *   *Note: Create the `scheduler` directory if it does not exist.*
2.  Move the result `enum class` or `struct`, the callback `using` alias, and any related `operator<<` overloads from the main command header to the new `_result.h` file.
3.  Update the original command header to include the new `_result.h` file.
4.  Identify all files that include the main command header.
5.  For files that only require the result types, change the include to point to the new `_result.h` file. The command's own `.cc` and `_unittest.cc` files will continue to include the main header.
6.  Update the corresponding `BUILD.gn` file to include the new `_result.h` file in the appropriate target.
7.  Check for `IfChange` markers in the moved code. If present:
    *   Update the path in the `ThenChange` marker in the new `_result.h` file to point to the correct file (e.g., `tools/metrics/histograms/metadata/webapps/enums.xml`).
    *   Update the corresponding `ThenChange` marker in the destination file (e.g., `tools/metrics/histograms/metadata/webapps/enums.xml`) to point to the new `_result.h` file location.

## 4. Key Files and Context

*   **Files:**
    *   `//chrome/browser/web_applications/BUILD.gn`
    *   `//chrome/browser/web_applications/commands/`
    *   `//chrome/browser/web_applications/isolated_web_apps/commands/`
    *   `//chrome/browser/web_applications/scheduler/`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/`
    *   All files listed in the "Files to be Modified" section of the investigation document.

## 5. TODO Log

**Log Key:**
*   `[ ]` - To Do
*   `[x]` - Done
*   `[SKIP]` - Skipped by the user
*   `[! FAILED]` - Failed, requires user input

### Phase 1: Refactor `commands` directory

*   [x] **Task 1.1:** Refactor `ApplyPendingManifestUpdateCommand`
    *   Create `chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h`.
    *   Move result types from `apply_pending_manifest_update_command.h`.
    *   Update includes in dependent files.
    *   Update `BUILD.gn`.
    *   Build and run tests.
*   [ ] **Task 1.2:** Refactor `FetchInstallInfoFromInstallUrlCommand`
    *   Target: `chrome/browser/web_applications/scheduler/fetch_install_info_from_install_url_result.h`
*   [ ] **Task 1.3:** Refactor `FetchInstallabilityForChromeManagement`
    *   Target: `chrome/browser/web_applications/scheduler/fetch_installability_for_chrome_management_result.h`
*   [ ] **Task 1.4:** Refactor `GeneratedIconFixCommand`
    *   Target: `chrome/browser/web_applications/scheduler/generated_icon_fix_result.h`
*   [ ] **Task 1.5:** Refactor `ManifestSilentUpdateCommand`
    *   Target: `chrome/browser/web_applications/scheduler/manifest_silent_update_result.h`
*   [ ] **Task 1.6:** Refactor `NavigateAndTriggerInstallDialogCommand`
    *   Target: `chrome/browser/web_applications/scheduler/navigate_and_trigger_install_dialog_result.h`
*   [ ] **Task 1.7:** Refactor `RewriteDiyIconsCommand`
    *   Target: `chrome/browser/web_applications/scheduler/rewrite_diy_icons_result.h`
*   [ ] **Task 1.8:** Refactor `RunOnOsLoginCommand`
    *   Target: `chrome/browser/web_applications/scheduler/run_on_os_login_result.h`
*   [ ] **Task 1.9:** Refactor `WebAppIconDiagnosticCommand`
    *   Target: `chrome/browser/web_applications/scheduler/web_app_icon_diagnostic_result.h`

### Phase 2: Refactor `isolated_web_apps/commands` directory

*   [ ] **Task 2.1:** Refactor `CheckIsolatedWebAppBundleUserInstallabilityCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/check_isolated_web_app_bundle_installability_result.h`
*   [ ] **Task 2.2:** Refactor `CleanupBundleCacheCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/cleanup_bundle_cache_result.h`
*   [ ] **Task 2.3:** Refactor `CleanupOrphanedIsolatedWebAppsCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/cleanup_orphaned_isolated_web_apps_result.h`
*   [ ] **Task 2.4:** Refactor `CopyBundleToCacheCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/copy_bundle_to_cache_result.h`
*   [ ] **Task 2.5:** Refactor `GetBundleCachePathCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/get_bundle_cache_path_result.h`
*   [ ] **Task 2.6:** Refactor `InstallIsolatedWebAppCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/install_isolated_web_app_result.h`
*   [ ] **Task 2.7:** Refactor `IsolatedWebAppApplyUpdateCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/isolated_web_app_apply_update_result.h`
*   [ ] **Task 2.8:** Refactor `IsolatedWebAppPrepareAndStoreUpdateCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/isolated_web_app_prepare_and_store_update_result.h`
*   [ ] **Task 2.9:** Refactor `RemoveObsoleteBundleVersionsCacheCommand`
    *   Target: `chrome/browser/web_applications/isolated_web_apps/scheduler/remove_obsolete_bundle_versions_cache_result.h`

### Unresolved Issues
*(Log of failed tasks and blocking issues)*
