---
id: 2026-09-18-manifest-validating-type
title: 'Plan: Validating Web App Manifest Mojo Type'
project: components/webapps
author: dmurph@chromium.org, AI Assistant
status: in-progress
date: '2026-09-18'
design_doc: ../designs/2026-09-18-manifest-validating-type-design.md
bug: crbug.com/485890278
---

<!--
**Agent Preamble:**
> **CRITICAL:** Before executing any milestone, you MUST read the project's
> `AGENTS.md` (if it exists) to understand operating procedures. Run
> `git cl format` and `git cl presubmit -u --force` before completing each
> milestone. Proceed to next milestone only after user confirmation.
-->

# Execution Plan: Validating Web App Manifest Mojo Type

## 1. Purpose / Big Picture

Execute the
[Technical Design](../designs/2026-09-18-manifest-validating-type-design.md)
(`crbug.com/485890278`) across a sequence of small, self-contained Gerrit CLs
that promote
[`blink::Manifest`](/third_party/blink/public/common/manifest/manifest.h) into a
validating C++ value class and typemap `blink.mojom.Manifest` ->
`::blink::Manifest` in `cpp_typemaps` without breaking the build at any
intermediate step.

## 2. Context and Orientation

Directly adding `blink.mojom.Manifest` to `cpp_typemaps` in
[`/third_party/blink/public/mojom/BUILD.gn`](/third_party/blink/public/mojom/BUILD.gn)
would force ~147 non-renderer files across Blink, Content, Components, and
Chrome to change in a single CL.

To keep each CL small and independently reviewable:

1. **Preconditions & Wire Nullability First (Milestones 1–2):** Land the `C0`
   `ParseManifestFromString` precondition and nullable `Manifest?` Mojo
   responses before changing C++ types.
2. **Type + Temporary Bridges Without `BUILD.gn` Typemap (Milestone 3):**
   Introduce the encapsulating `blink::Manifest` class, `Manifest::Builder`, the
   8 new nested C++ structs and their `StructTraits`, and temporary
   `blink::ManifestFromMojom` / `blink::ManifestToMojom` helpers while leaving
   `third_party/blink/public/mojom/BUILD.gn` unmapped so existing consumers
   continue to compile.
3. **Host Validation First, Then Outward Consumer Migration (Milestones 4–12):**
   Switch
   [`content::ManifestManagerHost`](/content/browser/manifest/manifest_manager_host.cc)
   internally to `blink::Manifest`, placing `ManifestToMojom` shims at its
   outgoing public boundaries. Each subsequent milestone migrates one API
   boundary and its subsystem consumers, pushing the temporary shims outward
   until zero non-renderer consumers use `blink::mojom::ManifestPtr`.
4. **Activate `cpp_typemaps` & Delete Bridges Last (Milestone 13):** Register
   `blink.mojom.Manifest` and the 8 nested structs in `cpp_typemaps`, move
   intrinsic validation into `StructTraits::Read()`, and delete
   `ManifestFromMojom`, `ManifestToMojom`, and `blink::IsEmptyManifest`.

## 3. Progress

- [x] **Milestone 1: Enforce Precondition `C0` in
  `ManifestManagerHost::ParseManifestFromString`**
- [x] **Milestone 2: Make `Manifest?` Nullable in `ManifestManager` Mojo IPCs**
- [ ] **Milestone 3: Implement `blink::Manifest` Class, `Builder`, Nested Struct
  Traits, and Bridges**
- [ ] **Milestone 4: Switch `ManifestManagerHost` Internals to `blink::Manifest`
  and Contextual Checks `C1`–`C5`**
- [ ] **Milestone 5: Migrate `ContentBrowserClient::MaybeOverrideManifest` and
  Embedder Overrides**
- [ ] **Milestone 6: Migrate `RequestManifestDebugInfo` and DevTools
  `PageHandler`**
- [ ] **Milestone 7: Migrate `content::Page::GetManifest` and
  `ContentFaviconDriver`**
- [ ] **Milestone 8: Migrate `//components/webapps/browser/installable/`**
- [ ] **Milestone 9: Migrate `//components/webapps/browser/banners/`,
  `WebappsClient`, and Android `ShortcutInfo` / `WebAPK`**
- [ ] **Milestone 10: Migrate `content::PageManifestManager`,
  `ParseManifestFromStringJob`, and `//chrome/browser/web_applications/jobs/`**
- [ ] **Milestone 11: Migrate `WebAppDataRetriever`, `PrepareInstallInfoJob`,
  `WebAppCommandScheduler`, UI, and Commands**
- [ ] **Milestone 12: Migrate `FakeWebContentsManager`, `IsolatedWebAppBuilder`,
  and WebApp Unit/Browser Tests**
- [ ] **Milestone 13: Activate `blink.mojom.Manifest` Mojo Typemap, Remove
  Bridges, and Run Harness Freshness Audit**

______________________________________________________________________

## 4. Surprises & Discoveries

_(Document unexpected behaviors, optimizations, or bugs discovered during
implementation)_

______________________________________________________________________

## 5. Decision Log

- **2026-09-18:** Defer registering all 9 `cpp_typemaps` entries
  (`blink.mojom.Manifest` + the 8 nested structs) in
  [`/third_party/blink/public/mojom/BUILD.gn`](/third_party/blink/public/mojom/BUILD.gn)
  until Milestone 13. Registering the 8 nested struct typemaps early in
  Milestone 3 would change the generated C++ field types inside
  `blink::mojom::Manifest` and break ~35+ unmigrated files.
- **2026-09-18:** Migrate `Page::GetManifest` and `RequestManifest` in
  Milestones 2 and 7 rather than blocking on `crbug.com/452053908`, since
  `ContentFaviconDriver` and `InstallableDataFetcher` still call
  `Page::GetManifest` today.
- **2026-09-18:** Keep `FakeWebContentsManager::FakePageState` (`opt_manifest`
  and `manifest_before_default_processing`) as `blink::mojom::ManifestPtr`
  through Milestones 10–11 (converting internally via `ManifestFromMojom`), and
  migrate `FakePageState` and its ~25 test files in Milestone 12 so production
  job/command CLs stay small and focused.

______________________________________________________________________

## 6. Plan of Work (Milestones)

### Milestone 1: Enforce Precondition `C0` in `ManifestManagerHost::ParseManifestFromString`

**Goal:** Reject invalid or opaque-origin `document_url` inputs in
`ManifestManagerHost::ParseManifestFromString` before sending the Mojo IPC
(Design Doc §2 `C0`).

- **Scope & Boundaries:**
  - In
    [`/content/browser/manifest/manifest_manager_host.cc`](/content/browser/manifest/manifest_manager_host.cc),
    if `!document_url.is_valid() || url::Origin::Create(document_url).opaque()`,
    post `callback` asynchronously with an empty manifest (which becomes
    `std::nullopt` in Milestone 10) without contacting the renderer.
  - Document the precondition in
    [`/content/public/browser/page_manifest_manager.h`](/content/public/browser/page_manifest_manager.h)
    and add test coverage in
    [`/content/browser/manifest/manifest_browsertest.cc`](/content/browser/manifest/manifest_browsertest.cc).
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc
  ```

______________________________________________________________________

### Milestone 2: Make `Manifest?` Nullable in `ManifestManager` Mojo IPCs

**Goal:** Update `RequestManifest` and `RequestManifestDebugInfo` in
[`/third_party/blink/public/mojom/manifest/manifest_manager.mojom`](/third_party/blink/public/mojom/manifest/manifest_manager.mojom)
to return nullable `Manifest?` so the renderer sends `null` where it previously
sent an empty `Manifest::New()` (`kUnexpectedFailure` and `kNoManifestAllowed`).

- **Scope & Boundaries:**
  - Renderer
    ([`/third_party/blink/renderer/modules/manifest/manifest_manager.cc`](/third_party/blink/renderer/modules/manifest/manifest_manager.cc)):
    Allow `Result::manifest_` to be null; send `null` on `kUnexpectedFailure`
    and `kNoManifestAllowed`, while keeping `DefaultManifest()` unchanged for
    `kNoManifestSpecified`, `kManifestFailedToFetch`, and
    `kManifestFailedToParse`.
  - Browser bridges: In
    [`/content/browser/manifest/manifest_manager_host.cc`](/content/browser/manifest/manifest_manager_host.cc),
    map a null `manifest` back to `Manifest::New()` for
    `Page::GetManifestCallback` callers until Milestone 7 (while reporting a bad
    message if `result == kSuccess && !manifest`). In
    [`/content/browser/devtools/protocol/page_handler.cc`](/content/browser/devtools/protocol/page_handler.cc)
    (`GotManifest`), guard the `input_manifest` field-reading block when
    `input_manifest` is null.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default third_party/blink/renderer/modules/manifest/manifest_manager_test.cc content/browser/manifest/manifest_browsertest.cc
  ```

______________________________________________________________________

### Milestone 3: Implement `blink::Manifest` Class, `Builder`, Nested Struct Traits, and Bridges

**Goal:** Introduce the validating `blink::Manifest` C++ value class,
`Manifest::Builder`, `Manifest::CreateDefault`,
`IsDefaultManifest(const Manifest&, ...)`, the 8 new nested C++ structs and
their `StructTraits`, `ManifestInvariantError` (`I1`–`I13`) + UMA histograms,
and temporary `ManifestFromMojom` / `ManifestToMojom` conversion helpers (Design
Doc §2 & §6).

- **Scope & Boundaries:**
  - [`/third_party/blink/public/common/manifest/`](/third_party/blink/public/common/manifest/)
    and
    [`/third_party/blink/common/manifest/`](/third_party/blink/common/manifest/):
    Implement `blink::Manifest` (all fields of `manifest.mojom` +
    `GetOrigin()`), `Manifest::Builder` (`Build()` and `BuildChecked()`),
    `Manifest::CreateDefault`, `ManifestFromMojom`, `ManifestToMojom`, and
    `StructTraits` for the 8 unmapped nested structs (`Screenshot`,
    `FileHandler`, `ProtocolHandler`, `ScopeExtension`, `LockScreen`,
    `NoteTaking`, `MigrateFrom`, `MigrateTo`).
  - Leave
    [`/third_party/blink/public/mojom/BUILD.gn`](/third_party/blink/public/mojom/BUILD.gn)
    unchanged so no existing consumers break.
  - Add unit tests in `manifest_unittest.cc`,
    `manifest_mojom_traits_unittest.cc`, `manifest_util_unittest.cc`, and
    `manifest_manager_test.cc` covering `I1`–`I13`, `CreateDefault` parity with
    the renderer, full-field round-trip conversion, and direct
    `SerializeAndDeserialize` of the 8 nested struct traits.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default third_party/blink/common/manifest/manifest_unittest.cc third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc third_party/blink/common/manifest/manifest_util_unittest.cc third_party/blink/renderer/modules/manifest/manifest_manager_test.cc
  ```

______________________________________________________________________

### Milestone 4: Switch `ManifestManagerHost` Internals to `blink::Manifest` and Contextual Checks `C1`–`C5`

**Goal:** Validate all incoming renderer manifests in
[`/content/browser/manifest/manifest_manager_host.cc`](/content/browser/manifest/manifest_manager_host.cc)
using `ManifestFromMojom` (`I1`–`I13`) and Contextual Checks `C1`–`C5` (Design
Doc §2), verify exact parity with `MaybeGetBadMessageStringForManifest`, delete
`MaybeGetBadMessageStringForManifest`, and notify
`developer_manifest_callback_list_` / `all_manifests_callback_list_` with
`kUnexpectedFailure` on `OnConnectionError`.

- **Scope & Boundaries:**
  - Internal to [`/content/browser/manifest/`](/content/browser/manifest/).
    Public `content/` signatures remain unchanged in this CL by converting
    validated `blink::Manifest` instances back via `ManifestToMojom` at outgoing
    callback/hook boundaries.
  - Run the temporary parity test in `manifest_browsertest.cc` confirming 100%
    agreement between `MaybeGetBadMessageStringForManifest` and the new checks
    before deleting `MaybeGetBadMessageStringForManifest`.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc
  ```

______________________________________________________________________

### Milestone 5: Migrate `ContentBrowserClient::MaybeOverrideManifest` and Embedder Overrides

**Goal:** Migrate `ContentBrowserClient::MaybeOverrideManifest` to
`std::optional<blink::Manifest> MaybeOverrideManifest(RenderFrameHost*, const blink::Manifest&)`
(Design Doc §2).

- **Scope & Boundaries:**
  - Update
    [`/content/public/browser/content_browser_client.h`](/content/public/browser/content_browser_client.h),
    [`/content/browser/manifest/manifest_manager_host.cc`](/content/browser/manifest/manifest_manager_host.cc)
    (with `CHECK(overridden->GetOrigin() == manifest->GetOrigin())`),
    [`/chrome/browser/chrome_content_browser_client.cc`](/chrome/browser/chrome_content_browser_client.cc)
    (explicit chaining),
    [`/chrome/browser/web_applications/chromeos_web_app_experiments.cc`](/chrome/browser/web_applications/chromeos_web_app_experiments.cc)
    (`BuildChecked()`), and
    [`/chrome/browser/web_applications/policy/web_app_policy_manager.cc`](/chrome/browser/web_applications/policy/web_app_policy_manager.cc)
    (`Build()` with `WebApp.Manifest.InvariantViolation.PolicyOverride` UMA
    fallback).
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/web_applications/policy/web_app_policy_manager_browsertest.cc content/browser/manifest/manifest_browsertest.cc
  autoninja -C out/CrOS browser_tests
  ```

______________________________________________________________________

### Milestone 6: Migrate `RequestManifestDebugInfo` and DevTools `PageHandler`

**Goal:** Migrate `ManifestManagerHost::ReportManifestDebugInfoCallback` and
`GotManifest` in
[`/content/browser/devtools/protocol/page_handler.cc`](/content/browser/devtools/protocol/page_handler.cc)
to `const std::optional<blink::Manifest>&`.

- **Scope & Boundaries:**
  - Removes the `ManifestToMojom` bridge on `OnRequestManifestDebugInfoResponse`
    in
    [`/content/browser/manifest/manifest_manager_host.cc`](/content/browser/manifest/manifest_manager_host.cc)
    and updates `GotManifest` in `page_handler.cc` to use `blink::Manifest`
    accessors.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc
  ```

______________________________________________________________________

### Milestone 7: Migrate `content::Page::GetManifest` and `ContentFaviconDriver`

**Goal:** Migrate `content::Page::GetManifestCallback`
([`/content/public/browser/page.h`](/content/public/browser/page.h)) to
`std::optional<blink::Manifest>` and update `ContentFaviconDriver` and test
callers.

- **Scope & Boundaries:**
  - Removes the `ManifestToMojom` bridge on `OnRequestManifestResponse` in
    `ManifestManagerHost`.
  - Updates
    [`/components/favicon/content/content_favicon_driver.cc`](/components/favicon/content/content_favicon_driver.cc)
    and test callers of `Page::GetManifest`.
  - Places a temporary 1-line `ManifestToMojom` shim inside
    `InstallableDataFetcher::OnDidGetManifest`
    ([`/components/webapps/browser/installable/installable_data_fetcher.cc`](/components/webapps/browser/installable/installable_data_fetcher.cc))
    so `//components/webapps` is migrated cleanly in Milestone 8.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc components/favicon/content/content_favicon_driver_unittest.cc chrome/browser/web_applications/policy/web_app_policy_manager_browsertest.cc
  ```

______________________________________________________________________

### Milestone 8: Migrate `//components/webapps/browser/installable/`

**Goal:** Migrate the `//components/webapps/browser/installable/` subsystem
(`InstallablePageData`, `InstallableData`, `InstallableDataFetcher`,
`InstallableIconFetcher`, `InstallableEvaluator`, `InstallableTask`,
`InstallableManager`, `FakeInstallableManager`, `SiteManifestMetricsTask`,
`MLInstallabilityPromoter`) to `blink::Manifest` and
`std::optional<blink::Manifest>`, eliminating empty-manifest sentinels.

- **Scope & Boundaries:**
  - Removes the temporary `ManifestToMojom` shim in `InstallableDataFetcher` and
    replaces empty `Manifest::New()` sentinels in `InstallablePageData` and
    `InstallableTask::ResetWithError` with `std::optional<blink::Manifest>` /
    nullable `const blink::Manifest*`.
  - Places temporary 1-line `ManifestToMojom` shims at the 4 external
    `InstallableData` boundaries (`AppBannerManager::OnDidGetManifest`,
    `AddToHomescreenDataFetcher::OnDidGetManifestAndIcons`,
    `WebApkUpdateDataFetcher::OnDidGetInstallableData`, and
    `WebAppDataRetriever::OnDidPerformInstallableCheck`).
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default components/webapps/browser/installable/installable_evaluator_unittest.cc chrome/browser/webapps/installable/installable_manager_browsertest.cc
  autoninja -C out/Android components_unittests chrome_public_test_apk
  ```

______________________________________________________________________

### Milestone 9: Migrate `//components/webapps/browser/banners/`, `WebappsClient`, and Android `ShortcutInfo` / `WebAPK`

**Goal:** Migrate `AppBannerManager`, `WebAppBannerData`, `WebappsClient`,
`AppBannerManagerDesktop`, and all Android banner / `ShortcutInfo` / `WebAPK`
consumers (`//components/webapps/browser/android/`, `WebApkUpdateDataFetcher`)
to `blink::Manifest`.

- **Scope & Boundaries:**
  - Removes 3 of the 4 `InstallableData` shims from Milestone 8
    (`AppBannerManager`, `AddToHomescreenDataFetcher`,
    `WebApkUpdateDataFetcher`), leaving only
    `WebAppDataRetriever::OnDidPerformInstallableCheck` for Milestone 11 and a
    1-line `ManifestToMojom` shim in `WebappsClientDesktop::OnManifestSeen` for
    `VisitedManifestManager` (removed in Milestone 10).
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/banners/app_banner_manager_browsertest.cc
  autoninja -C out/Android components_unittests chrome_public_test_apk
  ```

______________________________________________________________________

### Milestone 10: Migrate `content::PageManifestManager`, `ParseManifestFromStringJob`, and `//chrome/browser/web_applications/jobs/`

**Goal:** Migrate `content::PageManifestManager`
([`/content/public/browser/page_manifest_manager.h`](/content/public/browser/page_manifest_manager.h)),
`WebContentsManager`, `WebAppTabHelper`, `ManifestUpdateManager`,
`VisitedManifestManager`, `PopulateFileHandlerInfoFromManifest`, and the
`//chrome/browser/web_applications/jobs/` layer (`ParseManifestFromStringJob`,
`ManifestToWebAppInstallInfoJob`, `ManifestUpdateJob`,
`MigrationTargetInstallJob`) to `blink::Manifest`.

- **Scope & Boundaries:**
  - Removes all remaining `ManifestToMojom` calls from `ManifestManagerHost` and
    `WebappsClientDesktop`.
  - Updates `ParseManifestFromStringJob` and its 3 callers
    (`ParseManifestFromManifestUrlCommand`,
    `InstallAppFromVerifiedManifestCommand`, `WebInstallServiceImpl`).
  - Adds a `GenerateAppIdFromManifest(const blink::Manifest&)` overload
    alongside the legacy mojom overload, and places temporary
    `ManifestFromMojom` bridges inside `FakeWebContentsManager`
    (`GetPrimaryPage*SpecifiedManifest`) and unmigrated command callers of
    `ManifestToWebAppInstallInfoJob` / `ManifestUpdateJob` /
    `MigrationTargetInstallJob` so commands and `FakePageState` tests can be
    migrated separately in Milestones 11–12.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc chrome/browser/web_applications/web_contents/web_app_data_retriever_unittest.cc chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job_unittest.cc chrome/browser/web_applications/jobs/migration_target_install_job_unittest.cc chrome/browser/web_applications/visited_manifest_manager_unittest.cc chrome/browser/web_applications/commands/parse_manifest_from_manifest_url_command_browsertest.cc
  ```

______________________________________________________________________

### Milestone 11: Migrate `WebAppDataRetriever`, `PrepareInstallInfoJob`, `WebAppCommandScheduler`, UI, and Commands

**Goal:** Migrate `WebAppDataRetriever::CheckInstallabilityCallback`,
`PrepareInstallInfoJob`, `WebAppCommandScheduler`, `WebAppUiManager`,
`WebAppDialogUtils`, and all remaining command classes under
`//chrome/browser/web_applications/commands/` to `blink::Manifest` /
`std::optional<blink::Manifest>`.

- **Scope & Boundaries:**
  - Removes the temporary `ManifestToMojom` shim in
    `WebAppDataRetriever::OnDidPerformInstallableCheck` and the temporary
    `ManifestFromMojom` bridges in the command classes.
  - Keeps `FakeWebContentsManager::FakePageState` (`opt_manifest` and
    `manifest_before_default_processing`) bridged internally via
    `ManifestFromMojom` so the ~25 unit tests that populate `FakePageState` are
    migrated cleanly in Milestone 12.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/web_applications/web_contents/web_app_data_retriever_unittest.cc chrome/browser/web_applications/commands/fetch_manifest_and_install_command_unittest.cc chrome/browser/web_applications/commands/manifest_silent_update_command_unittest.cc chrome/browser/web_applications/commands/external_app_resolution_command_unittest.cc
  ```

______________________________________________________________________

### Milestone 12: Migrate `FakeWebContentsManager`, `IsolatedWebAppBuilder`, and WebApp Unit/Browser Tests

**Goal:** Migrate `FakeWebContentsManager::FakePageState`, `FakeDataRetriever`,
`IsolatedWebAppBuilder`, and all remaining unit and browser tests in
`//chrome/browser/` from `blink::mojom::ManifestPtr` to
`std::optional<blink::Manifest>` and `blink::Manifest::Builder`, and remove the
legacy `GenerateAppIdFromManifest(const blink::mojom::Manifest&)` overload.

- **Scope & Boundaries:**
  - Test-only and test-support migration across
    `//chrome/browser/web_applications/`,
    `//chrome/browser/ui/web_applications/`, and `//chrome/browser/ash/`. After
    this CL, zero consumers outside `ManifestManagerHost` use
    `blink::mojom::ManifestPtr`.
- **Verification:**
  ```bash
  tools/autotest.py -C out/Default chrome/browser/web_applications/commands/fetch_manifest_and_install_command_unittest.cc chrome/browser/web_applications/commands/manifest_silent_update_command_unittest.cc chrome/browser/web_applications/web_install_service_impl_unittest.cc chrome/browser/ui/web_applications/web_app_browsertest.cc
  autoninja -C out/CrOS unit_tests browser_tests
  ```

______________________________________________________________________

### Milestone 13: Activate `blink.mojom.Manifest` Mojo Typemap, Remove Bridges, and Run Harness Freshness Audit

**Goal:** Register `blink.mojom.Manifest` -> `::blink::Manifest` and the 8
nested struct typemaps in `cpp_typemaps` in
[`/third_party/blink/public/mojom/BUILD.gn`](/third_party/blink/public/mojom/BUILD.gn),
enforce `I1`–`I13` inside
`StructTraits<blink::mojom::ManifestDataView, ::blink::Manifest>::Read()`,
delete `ManifestFromMojom`, `ManifestToMojom`, and `blink::IsEmptyManifest`, and
update harness documentation.

- **Scope & Boundaries:**
  - Implement `StructTraits<blink::mojom::ManifestDataView, ::blink::Manifest>`
    in
    [`/third_party/blink/public/common/manifest/manifest_mojom_traits.h`](/third_party/blink/public/common/manifest/manifest_mojom_traits.h)
    / `.cc` and register all 9 `cpp_typemaps` entries in
    [`/third_party/blink/public/mojom/BUILD.gn`](/third_party/blink/public/mojom/BUILD.gn).
  - Update `ManifestManagerHost` to receive `blink::Manifest` /
    `std::optional<blink::Manifest>` directly from Mojo, removing
    `ManifestFromMojom` calls and leaving only contextual checks `C1`–`C5` and
    `MaybeOverrideManifest`.
  - Delete `ManifestFromMojom`, `ManifestToMojom`, `blink::IsEmptyManifest`, and
    the `IsDefaultManifest(const mojom::Manifest&, ...)` overload.
  - **Harness Freshness Audit:** Update
    [`/components/webapps/AGENTS.md`](/components/webapps/AGENTS.md),
    [`/third_party/blink/public/common/manifest/AGENTS.md`](/third_party/blink/public/common/manifest/AGENTS.md),
    [`/content/browser/manifest/AGENTS.md`](/content/browser/manifest/AGENTS.md),
    and
    [`/content/browser/manifest/README.md`](/content/browser/manifest/README.md)
    to reflect the validating `blink::Manifest` type and host contextual checks.
- **Verification:**
  ```bash
  git grep "mojom::ManifestPtr" -- ':!third_party/blink/renderer/' ':!components/webapps/_agents/'
  git grep "IsEmptyManifest" -- ':!components/webapps/_agents/'
  tools/autotest.py -C out/Default third_party/blink/common/manifest/manifest_unittest.cc third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc content/browser/manifest/manifest_browsertest.cc
  autoninja -C out/Default blink_common_unittests content_browsertests components_unittests unit_tests browser_tests
  autoninja -C out/Android components_unittests unit_tests chrome_public_test_apk
  autoninja -C out/CrOS unit_tests browser_tests
  git cl format
  git cl presubmit -u --force
  ```

______________________________________________________________________

## 7. Artifacts and Notes

- **Parent Design Document:**
  [`2026-09-18-manifest-validating-type-design.md`](../designs/2026-09-18-manifest-validating-type-design.md)
- **Tracking Bug:** `crbug.com/485890278`
- **Related Cleanup Bug:** `crbug.com/452053908` (`Page::GetManifest` removal)

## 8. Outcomes & Retrospective

_(To be filled upon completion of the final milestone)_
