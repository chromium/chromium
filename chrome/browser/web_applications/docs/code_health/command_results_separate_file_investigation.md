# Investigation: Separating Command Result Types

-   **Author(s):** dmurph
-   **Team:** Web Applications
-   **Status:** In Progress
-   **Last modified:** 2025-10-31 12:04
-   **Tracking Bug:** {Link to bug, e.g., crbug.com/123456}
-   **Design Doc:** N/A

## Subject

This investigation explores the possibility of refactoring web application commands to improve code health. Specifically, it looks at:
-   Identifying commands that define result types (`enum class`), callback types (`using`), and stream operators (`operator<<`) within their main header file.
-   Determining the feasibility of moving these result-related declarations into separate, more lightweight header files.
-   Find all files that include any of these command files, and determine if it's possible to change their include to be the new result class.

### Discovered Key Considerations

Any solution must ensure that the core logic of the commands remains unchanged. The primary goal is to refactor include-dependencies, not to alter functionality. The main benefit is improved build performance and better code organization, as components that only need to know about the *outcome* of a command won't need to depend on the full implementation details of the command itself.

### Discovered Ideas

The clear path forward, already implemented for `FetchManifestAndUpdateCommand`, is to create a parallel `_result.h` header for each command that currently co-locates its result types. This new header would contain the result enum, callback type definitions, and any related stream operators.

## Background

In the web apps command architecture, many commands are scheduled through the `WebAppCommandScheduler`. Clients of this scheduler often need to handle the result of a command, which requires them to include the header file defining the result types. Currently, these types are defined in the same header as the command class itself. This practice forces clients to include a heavyweight header with many transitive dependencies, even if they only need the result definitions. This can negatively impact build times and violates the principle of separating interface from implementation.

## Investigation

### Step 1 (2025-10-31): Identify potential command candidates
I started by getting a list of all header files in `chrome/browser/web_applications/commands/` using `list_dir`. This gave me a comprehensive list of files to analyze.

**Findings:** The directory contains a mix of command implementations (`.cc`), command headers (`.h`), and unit tests. I focused my attention on the `.h` files, as they contain the public interface of the commands.

### Step 2 (2025-10-31): Analyze command headers for bundled result types
My initial automated searches were overly broad and did not yield useful results, likely due to an issue with the code search tool or parameters. I adjusted my approach to manually inspect the most likely candidates based on the file list from Step 1.

**Findings:** I manually reviewed the 10 most promising command headers identified in the investigation document.

### Step 3 (2025-10-31): Refine candidate list and analyze findings
After reading the files, I discovered nuances that the initial automated search missed. The strict criteria of `enum class`, `using`, and `operator<<` were not universally met.

**Findings:**
-   `AppUpdateDataReadCommand` was found to not be a suitable candidate, as its callback returns an `UpdateMetadata` type, not a simple result enum.
-   `WebAppIconDiagnosticCommand` uses a `struct` for its result type, not an `enum class`, but still fits the spirit of the refactoring and is a good candidate.
-   Several other commands were missing either the `using` alias or the `operator<<` overload, but still define their result types in the main header and would benefit from this refactoring.

This manual analysis has refined the list of candidates from ten to nine, ensuring a more accurate and effective refactoring plan.

### Step 4 (2025-10-31): Identify Dependent Files
To understand the full scope of the refactoring, I performed a `code_search` for each of the nine candidate command headers to find all the files that include them. This helps identify which files will need to be updated to include the new `_result.h` files.

**Findings:**
The searches confirmed that the primary consumers of these headers are:
-   The command's own implementation (`.cc`) and test (`_unittest.cc` or `_browsertest.cc`) files. These will continue to include the main command header.
-   The `WebAppCommandScheduler` (`web_app_command_scheduler.cc`), which is a central hub for command execution. This is a key candidate for switching to the new result headers.
-   Various UI components, managers, and test drivers that need to know the result of a command without needing the full command definition.

This analysis provides a clear and complete list of files that will be impacted by the refactoring, which are also listed in the "Relevant Context" section.

### Step 5 (2025-10-31): Independent Verification of Candidates
To ensure the accuracy and completeness of this investigation, I performed an independent verification of the claims.

**Verification Process:**
1.  Listed all header files in `chrome/browser/web_applications/commands/`.
2.  Systematically read through all command header files to identify any that defined a local result type (`enum class` or `struct`) used in a completion callback. This was done to find any missed candidates.
3.  Specifically reviewed the 9 identified candidates and the excluded `AppUpdateDataReadCommand` to double-check the analysis.

**Findings:**
-   The verification confirms that the 9 commands listed in this document are the correct and only candidates for this specific refactoring.
-   Many other commands use result enums that are defined in shared, external headers (e.g., `webapps::InstallResultCode`), or their callbacks do not return a result (e.g., `base::OnceClosure`). These are correctly excluded from this refactoring effort.
-   The analysis that `AppUpdateDataReadCommand` is not a candidate and that `WebAppIconDiagnosticCommand` is a good candidate (despite using a `struct`) is also correct.

This verification confirms that the scope of the proposed refactoring is well-defined and accurate.

### Step 6 (2025-10-31): Verify Dependent Files
To ensure the "Files to be Modified" list was accurate, I performed a `code_search` for every identified candidate command header to find all inclusion sites.

**Verification Process:**
1.  For each of the 9 candidate headers, I executed a `code_search` looking for `#include` directives.
2.  I collated the results, filtering out each command's own implementation and test files.
3.  I compared this generated list against the "Files to be Modified" section in this document.

**Findings:**
-   The code searches confirmed that the list of dependent files is accurate. The primary consumers are the `WebAppCommandScheduler`, various UI components, and other managers within the web applications system.
-   The searches did not uncover any unexpected or high-risk dependencies, reinforcing the conclusion that this refactoring is a safe and contained effort.

This verification confirms that the scope of necessary code changes is well understood and correctly documented.

### Step 7 (2025-10-31): Analyze `isolated_web_apps/commands` directory
Following the same methodology as before, I analyzed the header files in the `chrome/browser/web_applications/isolated_web_apps/commands/` directory.

**Findings:**
This directory contains several commands that also define their own result types locally and would benefit from the same refactoring. I identified nine new candidates:
-   `check_isolated_web_app_bundle_user_installability_command.h`
-   `cleanup_bundle_cache_command.h`
-   `cleanup_orphaned_isolated_web_apps_command.h`
-   `copy_bundle_to_cache_command.h`
-   `get_bundle_cache_path_command.h`
-   `install_isolated_web_app_command.h`
-   `isolated_web_app_apply_update_command.h`
-   `isolated_web_app_prepare_and_store_update_command.h`
-   `remove_obsolete_bundle_versions_cache_command.h`

Several other files in this directory were reviewed and excluded because they either did not return a result (e.g., `garbage_collect_storage_partitions_command.h`), were helper functions (`get_controlled_frame_partition_command.h`), or used result types defined elsewhere (`get_isolated_web_app_browsing_data_command.h`).

### Step 8 (2025-10-31): Re-verify dependent files with complete results
After realizing that some previous code searches had returned only partial results, I re-ran all searches and paginated through the complete result sets to ensure a comprehensive list of dependent files.

**Findings:**
The complete results revealed several more inclusion sites than the initial searches, particularly for central commands like `install_isolated_web_app_command.h`. The "Files to be Modified" list has been updated to reflect this complete and accurate data. This step underscores the importance of ensuring that all pages of code search results are reviewed.

## Summary of Findings

The investigation confirmed that it is both feasible and beneficial to separate result types for numerous web app commands. Eighteen specific commands have been identified as candidates for this refactoring across both the general and isolated web app command directories. This change will align them with the best practice already established by `FetchManifestAndUpdateCommand` and `FetchManifestAndUpdateResult`.

The commands identified for refactoring are:
1.  `ApplyPendingManifestUpdateCommand`
2.  `FetchInstallInfoFromInstallUrlCommand`
3.  `FetchInstallabilityForChromeManagement`
4.  `GeneratedIconFixCommand`
5.  `ManifestSilentUpdateCommand`
6.  `NavigateAndTriggerInstallDialogCommand`
7.  `RewriteDiyIconsCommand`
8.  `RunOnOsLoginCommand`
9.  `WebAppIconDiagnosticCommand`
10. `CheckIsolatedWebAppBundleUserInstallabilityCommand`
11. `CleanupBundleCacheCommand`
12. `CleanupOrphanedIsolatedWebAppsCommand`
13. `CopyBundleToCacheCommand`
14. `GetBundleCachePathCommand`
15. `InstallIsolatedWebAppCommand`
16. `IsolatedWebAppApplyUpdateCommand`
17. `IsolatedWebAppPrepareAndStoreUpdateCommand`
18. `RemoveObsoleteBundleVersionsCacheCommand`

### Existing Testing Infrastructure
No significant testing infrastructure changes are needed, as this is a pure refactoring of header dependencies. Existing unit and browser tests for these commands will continue to be valid after the changes are made.

## Potential Solutions

### Solution Idea 1: Create separate `_result.h` files for each command

This is the recommended approach.

For each of the identified commands, a new header file will be created with a `_result.h` suffix. These new files will be placed in a dedicated `scheduler` subdirectory:
*   For commands in `//chrome/browser/web_applications/commands/`, the result file will go to `//chrome/browser/web_applications/scheduler/`.
*   For commands in `//chrome/browser/web_applications/isolated_web_apps/commands/`, the result file will go to `//chrome/browser/web_applications/isolated_web_apps/scheduler/`.

The result `enum` (or `struct`), callback `using` declaration, and `operator<<` overload will be moved from the command's header to this new file. The original command header will then be updated to include the new, lightweight result header. Finally, any call sites that previously included the full command header just for the result types will be updated to include the `_result.h` file instead, reducing the dependency chain.

This solution is low-risk, as it is a straightforward code organization task that doesn't alter logic. It directly addresses the problem of unnecessary header dependencies.

## Relevant Context

*   **Files:**
    *   `//chrome/browser/web_applications/commands/` (directory containing all relevant commands)
    *   `//chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h` (example of existing good pattern)
    *   `//chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h` (example of existing good pattern)
    *   `//chrome/browser/web_applications/scheduler/` (directory for general command results)
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/` (directory for IWA command results)
*   **Candidate Command Headers to be Refactored:**
    *   `apply_pending_manifest_update_command.h`
    *   `fetch_install_info_from_install_url_command.h`
    *   `fetch_installability_for_chrome_management.h`
    *   `generated_icon_fix_command.h`
    *   `manifest_silent_update_command.h`
    *   `navigate_and_trigger_install_dialog_command.h`
    *   `rewrite_diy_icons_command.h`
    *   `run_on_os_login_command.h`
    *   `web_app_icon_diagnostic_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/check_isolated_web_app_bundle_user_installability_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/cleanup_orphaned_isolated_web_apps_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h`
    *   `chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h`
*   **Files to be Created:**
    *   `//chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h`
    *   `//chrome/browser/web_applications/scheduler/fetch_install_info_from_install_url_result.h`
    *   `//chrome/browser/web_applications/scheduler/fetch_installability_for_chrome_management_result.h`
    *   `//chrome/browser/web_applications/scheduler/generated_icon_fix_result.h`
    *   `//chrome/browser/web_applications/scheduler/manifest_silent_update_result.h`
    *   `//chrome/browser/web_applications/scheduler/navigate_and_trigger_install_dialog_result.h`
    *   `//chrome/browser/web_applications/scheduler/rewrite_diy_icons_result.h`
    *   `//chrome/browser/web_applications/scheduler/run_on_os_login_result.h`
    *   `//chrome/browser/web_applications/scheduler/web_app_icon_diagnostic_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/check_isolated_web_app_bundle_installability_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/cleanup_bundle_cache_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/cleanup_orphaned_isolated_web_apps_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/copy_bundle_to_cache_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/get_bundle_cache_path_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/install_isolated_web_app_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/isolated_web_app_apply_update_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/isolated_web_app_prepare_and_store_update_result.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/scheduler/remove_obsolete_bundle_versions_cache_result.h`
*   **Files to be Modified:**
    *   `//chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate_unittest.cc`
    *   `//chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper.cc`
    *   `//chrome/browser/extensions/api/management/chrome_management_api_delegate_nonandroid.cc`
    *   `//chrome/browser/extensions/api/tabs/tabs_test.cc`
    *   `//chrome/browser/ui/ash/system_web_apps/system_web_app_icon_checker_impl.cc`
    *   `//chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_user_installability_checker.cc`
    *   `//chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_user_installability_checker.h`
    *   `//chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_browsertest.cc`
    *   `//chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.cc`
    *   `//chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h`
    *   `//chrome/browser/ui/views/web_apps/web_app_integration_test_driver.cc`
    *   `//chrome/browser/ui/views/web_apps/web_app_update_review_dialog.cc`
    *   `//chrome/browser/ui/web_applications/diagnostics/web_app_icon_health_checks.cc`
    *   `//chrome/browser/ui/web_applications/sub_apps_permissions_policy_browsertest.cc`
    *   `//chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.cc`
    *   `//chrome/browser/ui/webui/web_app_internals/iwa_internals_handler.cc`
    *   `//chrome/browser/web_applications/generated_icon_fix_manager.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/browser_navigator_iwa_browsertest.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_prepare_apply_update_browsertest.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/isolated_web_app_uninstall_browsertest.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/test/mock_iwa_install_command_wrapper.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/test/mock_iwa_install_command_wrapper.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_apply_task.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_apply_task.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.cc`
    *   `//chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h`
    *   `//chrome/browser/web_applications/manifest_update_manager.cc`
    *   `//chrome/browser/web_applications/web_app_command_scheduler.cc`
    *   `//chrome/browser/web_applications/web_app_command_scheduler.h`
    *   `//chrome/browser/web_applications/web_app_provider.cc`
    *   `//chrome/browser/web_applications/web_app_registrar.cc`
*   **Key Code Search Queries:**
    *   **To find where a specific command header is included:** `content:"#include \"chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h\""`
    *   **To find new candidates for this refactoring:**
        *   For `commands`: `f:chrome/browser/web_applications/commands/.*\.h$ (c:^(struct|class|enum).*Result OR c:^(struct|class|enum).*Success OR c:^(struct|class|enum).*Error)`
        *   For `isolated_web_apps/commands`: `f:chrome/browser/web_applications/isolated_web_apps/commands/.*\.h$ (c:^(struct|class|enum).*Result OR c:^(struct|class|enum).*Success OR c:^(struct|class|enum).*Error)`
*   **Grep patterns**: `grep -E "^(struct|class|enum).*(Result|Info|Error|Success) {"` (which can then be used with a directory)


## Document History

| Date       | Summary of changes                                              |
| -----------|---------------------------------------------------------------- |
| 2025-10-31 | Initial document creation and research                          |
| 2025-10-31 | Re-formatted document to use standard investigation template    |
| 2025-10-31 | Added detailed findings to each step of the investigation       |
| 2025-10-31 | Refined candidate list after manual file review                 |
| 2025-10-31 | Identified all dependent files to complete the investigation |
| 2025-10-31 | Added Step 5 to independently verify the candidate list.       |
| 2025-10-31 | Added Step 6 to verify the list of dependent files.            |
| 2025-10-31 | Added Step 7 to analyze the isolated_web_apps/commands dir.    |
| 2025-10-31 | Updated 'Files to be Modified' with isolated command results.  |
| 2025-10-31 | Improved the accuracy of the Key Code Search Queries.          |
| 2025-10-31 | Corrected second code search query to include isolated apps.   |
| 2025-10-31 | Split complex code search query into two for reliability.      |
| 2025-10-31 | Simplified code search queries after testing revealed issues.  |
| 2025-10-31 | Replaced failing search queries with a reliable manual process.|
| 2025-10-31 | Replaced manual process with a set of simpler, tested queries. |
| 2025-10-31 | Updated search queries with a tested, more precise version.    |
| 2025-10-31 | Added Step 8 and updated file list after full code search.     |
