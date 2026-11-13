---
id: 2026-11-10-per-source-management-data
title: 'Plan: Per-Source Web App Management Data'
project: components/webapps
author: japhet@chromium.org
status: draft
date: '2026-11-10'
design_doc: ../designs/2026-11-10-per-source-management-data-design.md
bug: b/383186222
---

<!--
**Agent Preamble:**

> Read the project's `AGENTS.md` (if present) before executing milestones.
> Run `git cl format` and `git cl presubmit -u --force` before completing
> each milestone. Flip `status` to `in-progress` when starting Milestone 1.
> Pause for user confirmation between milestones on standalone plans, or
> proceed autonomously when running under `harness-project`.
-->

# Execution Plan: Per-Source Web App Management Data

## 1. Purpose / Big Picture

Execute
[2026-11-10-per-source-management-data-design.md](../designs/2026-11-10-per-source-management-data-design.md)
(`b/383186222`) across six stacked Gerrit CLs. This work replaces split source
and config containers on `WebApp` with one invariant-enforcing `ManagementData`
container. After completion, `ManagementData` prevents source-config drift,
retires `NOTREACHED()` parse crashes, and upgrades LevelDB safely from v7 to v8.

## 2. Context and Orientation

The desktop web app registry lives in
[/chrome/browser/web_applications/](/chrome/browser/web_applications/). Today,
[/chrome/browser/web_applications/web_app.h](/chrome/browser/web_applications/web_app.h)
and
[/chrome/browser/web_applications/proto/web_app.proto](/chrome/browser/web_applications/proto/web_app.proto)
split source state across `sources` and `management_to_external_config_info`.
Notice that
[/chrome/browser/ui/views/web_apps/web_app_integration_test_driver.cc](/chrome/browser/ui/views/web_apps/web_app_integration_test_driver.cc)
compares iterators across two calls to `management_to_external_config_map()`.
That pattern prevents returning temporary bridge maps by value.

All local Linux x64 builds and test runs use `out/Debug` (`is_debug = true`,
`is_component_build = true`). ChromeOS-guarded paths
(`#if BUILDFLAG(IS_CHROMEOS)`) require mandatory compilation via UTR
(`linux-chromeos-rel`) or `out/CrOS` in Milestones 4 and 6, plus
`linux-chromeos-rel` CQ tryjobs before landing.

To keep every CL small and reviewable, we stack six focused milestones:

1. **Retire v6 upgrade flag (Milestone 1):** Remove
   `kWebAppUpgradeToDatabaseVersion6` so database version upgrades reach v7
   unconditionally.
2. **Add `ManagementData` schema and C++ model (Milestone 2):** Add the new
   protobuf schema, C++ model, `proto_extras` rules, and unit tests in
   isolation.
3. **Delete dead helpers and redundant uninstall rebuild (Milestone 3):** Remove
   unused `ExternalConfigMap` helpers and redundant map cloning before model
   changes.
4. **Migrate `WebApp`, registrar, and callers with v7 bridge (Milestone 4):**
   Replace `sources_` and `management_to_external_config_map_` with
   `ManagementData` while keeping v7 protobuf wire compatibility.
5. **Upgrade database to v8 and native `management_data` (Milestone 5):** Bump
   database version to 8, migrate v7 protos, update parse histograms, and add
   migration tests.
6. **Harness freshness and end-to-end verification (Milestone 6):** Update model
   documentation, audit `AGENTS.md`, check leftover symbols, and run full test
   suites.

## 3. Progress

- [ ] **Milestone 1: Retire `features::kWebAppUpgradeToDatabaseVersion6`**
- [ ] **Milestone 2: Add `web_app_management_data.proto` and `ManagementData`
  Model**
- [ ] **Milestone 3: Delete Dead Helpers and `RemoveInstallSourceJob` Map
  Rebuild**
- [ ] **Milestone 4: Migrate `WebApp`, `WebAppRegistrar`, and Callers (v7
  Bridge)**
- [ ] **Milestone 5: Upgrade `WebAppDatabase` to v8 and Native
  `management_data`**
- [ ] **Milestone 6: Verification and Harness Freshness**

## 4. Surprises & Discoveries

_(To be updated during implementation)_

## 5. Decision Log

- **2026-11-10:** Use a temporary v7 serialization bridge in Milestone 4 instead
  of updating callers and bumping the database version in one CL. This keeps C++
  model changes separate from v7-to-v8 storage migration and histogram updates.
- **2026-11-10:** Isolate dead-helper deletions and `RemoveInstallSourceJob`
  cleanup in Milestone 3 so reviewers verify behavioral parity before the
  `WebApp` refactor.
- **2026-11-11:** Keep `status: draft` until implementation starts in Milestone
  1 (flipping to `in-progress` then) while parent design sits at `proposed`.
- **2026-11-11:** Migrate `WebApp` and callers together in Milestone 4 without
  an intermediate `ExternalConfigMap` shim.
  `management_to_external_config_map()` returns `const ExternalConfigMap&`
  (paired with cross-call iterator comparisons in
  `web_app_integration_test_driver.cc`), preventing a safe by-value bridge
  without a dual shadow map. Meanwhile, `ManagementData::SetIsPlaceholder()` and
  `AddAdditionalPolicyId()` enforce per-source `CHECK`s immediately, requiring
  `FinalizeInstallOrUpdateJob` and `DedupeInstallUrlsCommand` to migrate
  atomically.

## 6. Plan of Work (Milestones)

### Milestone 1: Retire `features::kWebAppUpgradeToDatabaseVersion6`

**Goal:** Remove `features::kWebAppUpgradeToDatabaseVersion6` and
`declined_to_upgrade` so `MigrateDatabase()` upgrades uniformly through version
7\.

- **Concrete Steps:**
  - Remove `kWebAppUpgradeToDatabaseVersion6` declaration from
    [/chrome/common/chrome_features.h](/chrome/common/chrome_features.h).
  - Remove `kWebAppUpgradeToDatabaseVersion6` definition from
    [/chrome/common/chrome_features.cc](/chrome/common/chrome_features.cc).
  - Edit `MigrateDatabase()` in
    [/chrome/browser/web_applications/web_app_database.cc](/chrome/browser/web_applications/web_app_database.cc)
    to upgrade version 6 unconditionally and drop `declined_to_upgrade`.
  - Remove unused feature headers from
    [/chrome/browser/web_applications/web_app_database.cc](/chrome/browser/web_applications/web_app_database.cc).
  - Flip Plan frontmatter `status` to `in-progress` on starting execution.
- **Interfaces and Dependencies:**
  - `WebAppDatabase::MigrateDatabase()` continues to compare
    `state.metadata.version()` against `GetCurrentDatabaseVersion()` uniformly.
  - Repo-wide `git grep` shows no other callers reference
    `WebAppUpgradeToDatabaseVersion6`.
- **Validation and Acceptance:**
  - Run `git grep WebAppUpgradeToDatabaseVersion6` outside `_agents/` and
    observe zero hits.
  - Build with `autoninja -C out/Debug unit_tests browser_tests`.
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/web_applications/web_app_database_unittest.cc chrome/browser/web_applications/web_app_database_migration_unittest.cc`.
  - Run `git cl format` and `git cl presubmit -u --force`.

______________________________________________________________________

### Milestone 2: Add `web_app_management_data.proto` and `ManagementData` Model

**Goal:** Add the 14 per-source protobuf messages, `proto::ManagementData`, the
C++ `ManagementData` value class, and unit tests without altering `WebApp`.

- **Concrete Steps:**
  - Create `chrome/browser/web_applications/proto/web_app_management_data.proto`
    defining the 14 source messages and `ManagementData`.
  - Register `web_app_management_data.proto` in `proto_library("proto")` and
    `proto_extras("proto_extras")` inside
    [/chrome/browser/web_applications/proto/BUILD.gn](/chrome/browser/web_applications/proto/BUILD.gn).
  - Create `chrome/browser/web_applications/model/management_data.h` and
    `chrome/browser/web_applications/model/management_data.cc`.
  - Add `management_data.h` and `management_data.cc` to
    `source_set("web_applications")` in
    [/chrome/browser/web_applications/BUILD.gn](/chrome/browser/web_applications/BUILD.gn).
  - Create `chrome/browser/web_applications/model/management_data_unittest.cc`
    and add it to `source_set("web_applications_unit_tests")` in
    [/chrome/browser/web_applications/BUILD.gn](/chrome/browser/web_applications/BUILD.gn).
- **Interfaces and Dependencies:**
  - `management_data.h` forward-declares `proto::ManagementData` and avoids
    including generated protobuf headers.
  - `ManagementData` provides `HasSource()`, `HasAnySources()`, `GetSources()`,
    `entries()`, `AddSource()`, `RemoveSource()`, `IsPlaceholder()`,
    `SetIsPlaceholder()`, `GetInstallUrls()`, `HasInstallUrl()`,
    `AddInstallUrl()`, `RemoveInstallUrl()`, `AddAdditionalPolicyId()`,
    `GetAllAdditionalPolicyIds()`, `Parse()`, `ToProto()`, `AsDebugValue()`, and
    `operator<<`.
  - `ToProto()` calls `mutable_<source>()` for empty structs (`kSync`,
    `kUserInstalled`, IWA) to preserve field presence.
  - `ManagementData::Parse()` allows an empty `proto::ManagementData` so v8
    pending-uninstall apps keep parsing.
- **Validation and Acceptance:**
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/web_applications/model/management_data_unittest.cc`.
  - Verify tests cover all 14 sources, `GetSources()` parity, URL mutators,
    placeholder `CHECK`s, policy ID `CHECK`s, and `Parse()` errors.
  - Verify tests confirm `AsDebugValue()` output and `ToProto()` empty-struct
    presence.
  - Run `git cl format` and `git cl presubmit -u --force`.

______________________________________________________________________

### Milestone 3: Delete Dead Helpers and `RemoveInstallSourceJob` Map Rebuild

**Goal:** Remove unused `ExternalConfigMap` helpers and redundant map cloning in
`RemoveInstallSourceJob` while behavior remains unchanged.

- **Concrete Steps:**
  - Delete `AppsExistWithExternalConfigData()` from
    [/chrome/browser/web_applications/web_app_registrar.h](/chrome/browser/web_applications/web_app_registrar.h)
    and
    [/chrome/browser/web_applications/web_app_registrar.cc](/chrome/browser/web_applications/web_app_registrar.cc).
  - Delete `AppIdContainsAllUrls()` from
    [/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h](/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h)
    and
    [/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.cc](/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.cc).
  - Delete `VerifyAppIdContainsAllUrls` from
    [/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs_browsertest.cc](/chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs_browsertest.cc).
  - Delete the `modified_config_map` loop from
    [/chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.cc](/chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.cc).
  - Remove unused `<algorithm>` include from
    [/chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.cc](/chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.cc).
- **Interfaces and Dependencies:**
  - `WebApp::RemoveSource()` already erases entries from
    `management_to_external_config_map_`, making the loop in
    `RemoveInstallSourceJob` redundant.
  - Repo-wide `git grep` confirms `AppsExistWithExternalConfigData()` and
    `AppIdContainsAllUrls()` have zero production callers.
- **Validation and Acceptance:**
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/web_applications/commands/web_app_uninstall_command_unittest.cc chrome/browser/web_applications/externally_managed_app_manager_unittest.cc`.
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs_browsertest.cc`.
  - Run `git cl format` and `git cl presubmit -u --force`.

______________________________________________________________________

### Milestone 4: Migrate `WebApp`, `WebAppRegistrar`, and Callers (v7 Bridge)

**Goal:** Replace `sources_` and `management_to_external_config_map_` with
`ManagementData` across `WebApp`, registrar, commands, jobs, policy, and tests
using a v7 serialization bridge.

- **Concrete Steps:**
  - Import `web_app_management_data.proto` and add
    `optional ManagementData management_data = 84` in
    [/chrome/browser/web_applications/proto/web_app.proto](/chrome/browser/web_applications/proto/web_app.proto).
  - Keep `sources = 7` and `management_to_external_config_info = 52` active in
    [/chrome/browser/web_applications/proto/web_app.proto](/chrome/browser/web_applications/proto/web_app.proto)
    for v7 wire compatibility.
  - Update `LINT.IfChange(MemberVariables)` in
    [/chrome/browser/web_applications/web_app.h](/chrome/browser/web_applications/web_app.h)
    to replace `sources_` and `management_to_external_config_map_` with
    `management_data_`.
  - Remove `ExternalManagementConfig`, `ExternalConfigMap`,
    `management_to_external_config_map()`, and
    `SetWebAppManagementExternalConfigMap()` from `web_app.h` and
    [/chrome/browser/web_applications/web_app.cc](/chrome/browser/web_applications/web_app.cc).
  - Expose `management_data()`, `SetManagementData()`, `SetIsPlaceholder()`,
    `AddInstallUrl()`, `RemoveInstallUrl()`, `AddAdditionalPolicyId()`, and
    `AddExternalSourceInformation()` on `WebApp`.
  - Retain sync install-state `CHECK_NE` inside `WebApp::AddSource()`, update
    `operator==` and `GetHighestPrioritySource()`.
  - Remove `"sources"` and `"management_type_to_external_configuration_map"` and
    set `"management_data"` in
    `WebApp::AsDebugValueWithOnlyPlatformAgnosticFields()`.
  - Remove stale `sources_` count and `is_uninstalling_` comments from
    [/chrome/browser/web_applications/web_app.h](/chrome/browser/web_applications/web_app.h).
  - Update `ProtoToWebAppManagement()` in
    [/chrome/browser/web_applications/web_app_database_serialization.cc](/chrome/browser/web_applications/web_app_database_serialization.cc)
    to return `std::optional<WebAppManagement::Type>` with `std::nullopt` for
    unknown values.
  - Bridge `ParseWebAppProto()` and `WebAppToProto()` in
    `web_app_database_serialization.cc` between v7 protobuf fields and
    `ManagementData`.
  - Update registrar lookups in
    [/chrome/browser/web_applications/web_app_registrar.cc](/chrome/browser/web_applications/web_app_registrar.cc),
    `LooksLikePlaceholder()` in
    [/chrome/browser/web_applications/web_app_helpers.cc](/chrome/browser/web_applications/web_app_helpers.cc),
    and
    [/chrome/browser/web_applications/externally_managed_app_manager.cc](/chrome/browser/web_applications/externally_managed_app_manager.cc).
  - Update
    [/chrome/browser/web_applications/commands/dedupe_install_urls_command.cc](/chrome/browser/web_applications/commands/dedupe_install_urls_command.cc)
    and rename `WriteExternalConfigMapInfo()` to `WriteManagementData()` in
    [/chrome/browser/web_applications/jobs/finalize_install_or_update_job.h](/chrome/browser/web_applications/jobs/finalize_install_or_update_job.h)
    and `.cc`.
  - Update
    [/chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.cc](/chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.cc),
    [/chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.cc](/chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.cc),
    and
    [/chrome/browser/web_applications/policy/web_app_policy_manager.cc](/chrome/browser/web_applications/policy/web_app_policy_manager.cc).
  - Update
    [/chrome/browser/web_applications/test/web_app_test_utils.cc](/chrome/browser/web_applications/test/web_app_test_utils.cc)
    so `CreateRandomWebApp()` emits valid per-source fields and test helpers use
    new mutators.
  - Update
    [/chrome/browser/web_applications/web_app_unittest.cc](/chrome/browser/web_applications/web_app_unittest.cc)
    and rebaseline
    [/chrome/test/data/web_apps/empty_web_app.json](/chrome/test/data/web_apps/empty_web_app.json)
    and
    [/chrome/test/data/web_apps/sample_web_app.json](/chrome/test/data/web_apps/sample_web_app.json).
  - Update unit and browser test assertions and add `WriteManagementData()`
    `CHECK` tests in
    [/chrome/browser/web_applications/jobs/finalize_install_or_update_job_unittest.cc](/chrome/browser/web_applications/jobs/finalize_install_or_update_job_unittest.cc).
- **Interfaces and Dependencies:**
  - `RemoveWebAppJob` keeps `webapps::IsUserUninstall(uninstall_source_)` and
    `CHECK(app->CanUserUninstallWebApp())`. Inside that guard, it passes
    `app->management_data().GetInstallUrls(WebAppManagement::kDefault)` whenever
    `app->IsPreinstalledApp()` is true.
  - `WebApp::AddExternalSourceInformation()` calls `AddInstallUrl()`, then calls
    `SetIsPlaceholder()` only for `kPolicy` or `kKiosk` (enforcing
    `CHECK(!is_placeholder)` otherwise).
  - `WebAppRegistrar::HasExternalAppWithInstallSource()` queries
    `web_app->GetSources().Has(source)` to match prior behavior.
  - The temporary `ParseWebAppProto()` bridge parses `proto.sources()` first
    (keeping buckets `12` and `13`), then validates every entry's `install_urls`
    (`43`) and `additional_policy_ids` (`44`) before filtering. That order keeps
    `ParseWebAppProto_InvalidExternalManagementConfig` passing while skipping
    `std::nullopt`, orphan inactive sources, and disallowed source types
    (`kSync`, `kUserInstalled`, IWA). Repeated entries use last-entry-wins,
    applying `is_placeholder` only to `kPolicy`/`kKiosk` and
    `additional_policy_ids` only to `kWebAppStore`.
  - The temporary `WebAppToProto()` bridge writes `mutable_sources()` for all 14
    types and emits `ManagementToExternalConfigInfo` only for active external
    entries. It skips `kSync`, `kUserInstalled`, IWA types, and tag `84`. This
    preserves byte-exact `RandomWebApps` round-trip equality.
  - Test updates span
    [/chrome/browser/web_applications/web_app_registrar_unittest.cc](/chrome/browser/web_applications/web_app_registrar_unittest.cc),
    [/chrome/browser/web_applications/commands/dedupe_install_urls_command_unittest.cc](/chrome/browser/web_applications/commands/dedupe_install_urls_command_unittest.cc),
    [/chrome/browser/web_applications/externally_managed_app_manager_unittest.cc](/chrome/browser/web_applications/externally_managed_app_manager_unittest.cc),
    [/chrome/browser/web_applications/externally_managed_app_manager_browsertest.cc](/chrome/browser/web_applications/externally_managed_app_manager_browsertest.cc),
    [/chrome/browser/web_applications/policy/web_app_policy_manager_unittest.cc](/chrome/browser/web_applications/policy/web_app_policy_manager_unittest.cc),
    [/chrome/browser/web_applications/policy/web_app_policy_manager_browsertest.cc](/chrome/browser/web_applications/policy/web_app_policy_manager_browsertest.cc),
    and
    [/chrome/browser/ui/views/web_apps/web_app_integration_test_driver.cc](/chrome/browser/ui/views/web_apps/web_app_integration_test_driver.cc).
  - In `externally_managed_app_manager_unittest.cc` and
    `externally_managed_app_manager_browsertest.cc`, user-installed/sync
    assertions (`kUserInstalled` and `kSync`) check for absent external sources
    rather than empty `entries()`.
- **Validation and Acceptance:**
  - Run `git grep management_to_external_config_map` outside `_agents/` and
    observe zero hits.
  - Build targets via `autoninja -C out/Debug unit_tests browser_tests`.
  - Run
    `tools/autotest.py -C out/Debug --run-all chrome/browser/web_applications/web_app_unittest.cc chrome/browser/web_applications/web_app_registrar_unittest.cc chrome/browser/web_applications/jobs/finalize_install_or_update_job_unittest.cc chrome/browser/web_applications/commands/dedupe_install_urls_command_unittest.cc chrome/browser/web_applications/externally_managed_app_manager_unittest.cc chrome/browser/web_applications/policy/web_app_policy_manager_unittest.cc chrome/browser/web_applications/web_app_database_serialization_unittest.cc chrome/browser/web_applications/web_app_database_unittest.cc`.
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/web_applications/externally_managed_app_manager_browsertest.cc chrome/browser/web_applications/policy/web_app_policy_manager_browsertest.cc`.
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/ui/views/web_apps/web_app_integration_browsertest.cc --gtest_filter="*PolicyApp*"`.
  - Run
    `tools/autotest.py -C out/Debug chrome/browser/ui/views/web_apps/web_app_integration_browsertest_mac_win_linux.cc --gtest_filter="*CheckPolicyAppUninstallWorks*"`.
  - Compile ChromeOS `unit_tests` and `browser_tests` via `out/CrOS` or UTR. Run
    `vpython3 tools/utr/run.py -p chromium -B try -b linux-chromeos-rel -t unit_tests -t browser_tests compile`.
  - Run `git cl format` and `git cl presubmit -u --force`.

______________________________________________________________________

### Milestone 5: Upgrade `WebAppDatabase` to v8 and Native `management_data`

**Goal:** Upgrade `WebAppDatabase` schema to version 8, migrate v7 protos to
`management_data`, update metrics enums, and switch serialization natively.

- **Concrete Steps:**
  - Deprecate fields `7` and `52` inside
    [/chrome/browser/web_applications/proto/web_app.proto](/chrome/browser/web_applications/proto/web_app.proto).
  - Bump `GetCurrentDatabaseVersion()` to `8` and add
    `MigrateSourcesAndExternalConfigToManagementData()` in
    [/chrome/browser/web_applications/web_app_database.cc](/chrome/browser/web_applications/web_app_database.cc).
  - Expand the outdated `GetCurrentDatabaseVersion()` docstring in
    [/chrome/browser/web_applications/web_app_database.h](/chrome/browser/web_applications/web_app_database.h)
    through version 8.
  - Log stripped anomalies to `log_` and record
    `WebApp.Migrations.SourcesAndExternalConfigToManagementData` in
    `web_app_database.cc`.
  - Move `ProtoToWebAppManagement()` into an anonymous namespace in
    `web_app_database.cc`, and delete `WebAppManagementToProto()` from
    [/chrome/browser/web_applications/web_app_database_serialization.cc](/chrome/browser/web_applications/web_app_database_serialization.cc).
  - Update `WebAppToProto()` and `ParseWebAppProto()` in
    `web_app_database_serialization.cc` to serialize and validate
    `management_data` natively.
  - Rename `kObsolete_NoSources` and `kObsolete_NoSourcesAndNotUninstalling`,
    add `74` to `76`, and bump `kMaxValue` in
    [/chrome/browser/web_applications/web_app_database_serialization.h](/chrome/browser/web_applications/web_app_database_serialization.h).
  - Update `WebAppProtoParseResult` in
    [/tools/metrics/histograms/metadata/webapps/enums.xml](/tools/metrics/histograms/metadata/webapps/enums.xml)
    for `(Obsolete)` labels `12` and `13`, plus `74` to `76`.
  - Add `WebApp.Migrations.SourcesAndExternalConfigToManagementData` with
    `units="apps"`, `expires_after="2027-11-10"`, `dmurph@chromium.org`, and
    `pwa-team@google.com` to
    [/tools/metrics/histograms/metadata/webapps/histograms.xml](/tools/metrics/histograms/metadata/webapps/histograms.xml).
  - Rewrite `CreateWebAppProtoForTesting()`, missing/empty source checks, and
    `InvalidExternalManagementConfig` onto `management_data` in
    [/chrome/browser/web_applications/web_app_database_serialization_unittest.cc](/chrome/browser/web_applications/web_app_database_serialization_unittest.cc).
  - Migrate `CreateWebAppProtoForTesting()` and
    `CanDeleteNonUserInstallableApps` onto `mutable_management_data()` in
    [/chrome/browser/web_applications/web_app_sync_bridge_unittest.cc](/chrome/browser/web_applications/web_app_sync_bridge_unittest.cc).
  - Add v7-to-v8 migration tests and keep pre-v8 fixtures in
    [/chrome/browser/web_applications/web_app_database_migration_unittest.cc](/chrome/browser/web_applications/web_app_database_migration_unittest.cc).
- **Interfaces and Dependencies:**
  - `ParseWebAppProto()` checks
    `proto.has_sources() || proto.management_to_external_config_info_size() > 0`
    (`76`) before checking `!proto.has_management_data()` (`74`). Next, it calls
    `ManagementData::Parse()`, maps errors to buckets `43` and `44`, and checks
    `!HasAnySources() && !is_uninstalling()` (`75`).
  - `WebAppToProto()` sets
    `*local_data->mutable_management_data() = web_app.management_data().ToProto()`
    unconditionally to handle `is_uninstalling()` zero-source apps.
  - `MigrateSourcesAndExternalConfigToManagementData()` uses last-entry-wins for
    duplicate `management` values and drops orphan, disallowed, or invalid
    fields leniently.
  - Pre-v8 (versions `0` to `6`) tests in
    `web_app_database_migration_unittest.cc` use a local helper setting
    `sources` so chained migrations reach v8.
  - Deprecating `sources = 7` and `management_to_external_config_info = 52`
    inside `LINT.IfChange(MemberVariables)` without editing `web_app.h` requires
    `NO_IFTTT=...` in the Milestone 5 CL description.
- **Validation and Acceptance:**
  - Run `git grep WebAppManagementToProto` and
    `git grep ProtoToWebAppManagement` outside `_agents/` to confirm expected
    placement.
  - Run
    `tools/autotest.py -C out/Debug --run-all chrome/browser/web_applications/web_app_database_serialization_unittest.cc chrome/browser/web_applications/web_app_database_migration_unittest.cc chrome/browser/web_applications/web_app_database_unittest.cc chrome/browser/web_applications/web_app_sync_bridge_unittest.cc`.
  - Verify serialization tests cover empty-source uninstalling apps, `43`, `44`,
    `74`, `75`, and both `76` arms.
  - Verify migration tests cover all 14 sources, orphan cleanup, unknown enum
    safety, anomaly `log_` entries, and idempotency.
  - Run `git cl format` and `git cl presubmit -u --force`.

______________________________________________________________________

### Milestone 6: Verification and Harness Freshness

**Goal:** Synchronize model and agent harness docs, audit repo-wide call sites
and test suites, and verify cross-platform readiness.

- **Concrete Steps:**
  - Document `ManagementData` and the `Parse()`/`ToProto()` domain-model pattern
    in
    [/chrome/browser/web_applications/model/README.md](/chrome/browser/web_applications/model/README.md).
  - Link `model/README.md` under Canonical Docs inside
    [/chrome/browser/web_applications/AGENTS.md](/chrome/browser/web_applications/AGENTS.md).
  - Run `harness-updater` checks to verify markdown links, manifest paths, and
    size budgets.
  - Audit leftover legacy symbols across the repository via `git grep`.
  - Run desktop suites via
    `tools/autotest.py -C out/Debug --run-all chrome/browser/web_applications/ chrome/browser/ui/views/web_apps/`.
  - Test ChromeOS `unit_tests` and `browser_tests` via `out/CrOS`, trybots, or
    UTR. Run
    `vpython3 tools/utr/run.py -p chromium -B try -b linux-chromeos-rel -t unit_tests -t browser_tests compile-and-test`.
- **Interfaces and Dependencies:**
  - Follows doc freshness rules defined in
    [/chrome/browser/web_applications/AGENTS.md](/chrome/browser/web_applications/AGENTS.md)
    and [/components/webapps/AGENTS.md](/components/webapps/AGENTS.md).
- **Validation and Acceptance:**
  - Confirm
    `git grep -E "ExternalConfigMap|ExternalManagementConfig|management_to_external_config_map|WriteExternalConfigMapInfo"`
    outside `_agents/` and protobuf definitions returns zero hits.
  - Build cleanly via `autoninja -C out/Debug unit_tests browser_tests`.
  - Pass
    `tools/autotest.py -C out/Debug --run-all chrome/browser/web_applications/ chrome/browser/ui/views/web_apps/`.
  - Confirm ChromeOS targets build and pass tests via UTR or
    `linux-chromeos-rel` tryjobs before landing.
  - Run `git cl format` and `git cl presubmit -u --force` with zero warnings or
    errors.

## 7. Artifacts and Notes

- **Parent Design Document:**
  [2026-11-10-per-source-management-data-design.md](../designs/2026-11-10-per-source-management-data-design.md)
- **Tracking Bug:** `b/383186222`
- **Prototype Reference Bugs:** `b/409773874`, `b/401593568`

## 8. Outcomes & Retrospective

_(To be filled upon completion of the final milestone)_
