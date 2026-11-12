---
id: 2026-11-06-top-e2e-metrics
title: 'Plan: Top Edge-to-Edge Startup, Page-Load, and Blink UseCounter Metrics'
project: chrome/browser/ui/android/edge_to_edge
author: dmurph@chromium.org
status: completed
date: '2026-11-06'
design_doc: ../designs/2026-11-06-top-e2e-metrics-design.md
bug: b/580934118, b/567231650
---

<!--
**Agent Preamble:**

> Read the project's `AGENTS.md` (if present) before executing milestones.
> Run `git cl format` and `git cl presubmit -u --force` before completing
> each milestone. Pause for user confirmation between milestones on
> standalone plans, or proceed autonomously when running under
> `harness-project`.
-->

## 1. Purpose / Big Picture

This plan implements the complete Top Edge-to-Edge telemetry funnel across
`b/567231650` and `b/580934118` defined in the
[parent design](../designs/2026-11-06-top-e2e-metrics-design.md). After these
milestones complete:

1. Blink records `WebFeature::kViewportFitCoverAndSafeAreaInsetTop` (`6097`) and
   forwards it to UKM when an outermost main-frame document uses both
   `viewport-fit=cover` and `env(safe-area-inset-top)` in either order.
2. Chrome on Android records startup Top Edge-to-Edge eligibility
   (`Android.EdgeToEdge.Top.Eligible` and
   `Android.EdgeToEdge.Top.IneligibilityReason`), registers the symmetric
   `EdgeToEdgeTopEligibility` synthetic trial in
   `RootUiCoordinator.onFinishNativeInitialization()`, and prevents 3-button
   navigation top-only startups from skewing bottom-chin `OnCreateController`
   metrics.
3. Chrome on Android records `MobilePageLoadedWithTopToEdge` in
   `UmaSessionStats` and `Android.EdgeToEdge.TopScalp.PageLoadState` in
   `EdgeToEdgeControllerImpl` when a page finishes loading.

## 2. Context and Orientation

1. **Blink `UseCounter` and UKM (`Milestone 1`, `b/580934118`)**:
   - `/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom`:
     Defines `blink::mojom::WebFeature` enum entries (`6097` is the next
     available value after `6096`).
   - `/tools/metrics/histograms/metadata/blink/enums.xml`: Synced via
     `tools/metrics/histograms/update_use_counter_feature_enum.py`.
   - `/components/page_load_metrics/browser/observers/use_counter/ukm_features.cc`:
     Allowlists `WebFeature` values for UKM reporting (`kViewportFitCover` is at
     line 491).
   - `/third_party/blink/renderer/core/frame/viewport_data.cc`:
     `ViewportData::UpdateViewportDescription()` counts `kViewportFitCover` on
     the outermost main frame.
   - `/third_party/blink/renderer/core/css/document_style_environment_variables.cc`:
     `DocumentStyleEnvironmentVariables::RecordVariableUsage()` counts
     `kCSSEnvironmentVariable_SafeAreaInsetTop` outside User-Agent shadow DOM.
2. **Android Startup Eligibility and Synthetic Trial (`Milestone 2`,
   `b/567231650`)**:
   - `/chrome/android/java/src/org/chromium/chrome/browser/ui/RootUiCoordinator.java`:
     Records `recordEligibilityForEveryStart(mActivity)` in
     `onFinishNativeInitialization()` and `recordEligibilityOnCreate(mActivity)`
     \+ `EdgeToEdgeChinEligibility` in `initializeEdgeToEdgeController()`.
   - `/chrome/browser/ui/android/edge_to_edge/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeUtils.java`:
     Implements `recordEligibility` using `IneligibilityReason`.
3. **Android Page-Load UMA and UserAction (`Milestone 3`, `b/580934118`)**:
   - `/chrome/android/java/src/org/chromium/chrome/browser/metrics/UmaSessionStats.java`
     and `/chrome/browser/android/metrics/uma_session_stats.cc`: Record
     page-load user actions (`MobilePageLoadedWithToEdge`).
   - `/chrome/browser/ui/android/edge_to_edge/internal/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerImpl.java`:
     Observes the active tab via `mTabObserver` and owns Top Scalp state.

## 3. Progress

- [x] **Milestone 1: Blink `kViewportFitCoverAndSafeAreaInsetTop` UseCounter and
  UKM (`b/580934118`)**
- [x] **Milestone 2: Android Top E2E Startup Eligibility UMA and Synthetic Trial
  (`b/567231650`)**
- [x] **Milestone 3: Android `TopScalp.PageLoadState` UMA and
  `MobilePageLoadedWithTopToEdge` (`b/580934118`)**
- [x] **Milestone 4: Verification & Harness Freshness**

## 4. Surprises & Discoveries

1. **Symmetric `EdgeToEdgeTopEligibility` registration in
   `onFinishNativeInitialization()`**: Because `EdgeToEdgeControllerCreator`
   only invokes `initializeEdgeToEdgeController()` on 3-button navigation phones
   when `sEdgeToEdgeTopInset` is enabled, registering `EdgeToEdgeTopEligibility`
   inside `initializeEdgeToEdgeController()` would exclude 3-button navigation
   phones from the Control arm. Registering in `onFinishNativeInitialization()`
   when `supportsTopInset()` is true keeps Control and Enabled arms symmetric.
2. **Blink `StyleEnvironmentVariablesTest` viewport meta preconditions**:
   `ViewportData::GetViewportDescription()` ignores `<meta name="viewport">`
   unless `GetDocument().GetSettings()->SetViewportMetaEnabled(true)` is called,
   and `HTMLMetaElement::ProcessViewportKeyValuePair()` requires
   `ScopedDisplayCutoutAPIForTest(true)`.
3. **Refactor-only arm on customized NTPs**:
   `EdgeToEdgeUtils.tabSupportsTopEdgeToEdge(tab)` returns `true` for customized
   NTPs when `isEdgeToEdgeRefactorEnabled()` is true even if
   `isTopEdgeToEdgeEnabled()` is false. Gating `MobilePageLoadedWithTopToEdge`
   on
   `EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() && EdgeToEdgeUtils.tabSupportsTopEdgeToEdge(tab)`
   prevents emissions when Top Edge-to-Edge is disabled.
4. **Custom Tabs exclusion**: `BaseCustomTabRootUiCoordinator` initializes
   `EdgeToEdgeControllerImpl` for CCT media viewer on phones without creating
   Top Scalp. Guarding `recordTopScalpPageLoadState(tab)` with
   `!tab.isCustomTab()` excludes Custom Tabs.

## 5. Decision Log

1. **2026-11-06 — Include `b/567231650` alongside `b/580934118`**: Approved by
   `dmurph@chromium.org`. Combining both bugs into one design and stacked plan
   covers the full Top Edge-to-Edge metrics funnel and coordinates edits to
   `EdgeToEdgeUtils.java`, `histograms.xml`, and test files.
2. **2026-11-06 — Guard bottom-chin `OnCreateController` metrics on top-only
   startups and omit top `.OnCreateController` variants**: Approved by
   `dmurph@chromium.org`. Unlike bottom-chin eligibility (which depends on
   asynchronous window insets for `hasTappableNavigationBar`), top eligibility
   depends only on static device properties known at
   `onFinishNativeInitialization()`, making top `.OnCreateController`
   ineligibility unreachable when `sEdgeToEdgeMonitorConfigurations` is enabled.
   In `RootUiCoordinator.initializeEdgeToEdgeController()`, gate bottom-chin
   `recordEligibilityOnCreate` and `EdgeToEdgeChinEligibility` so top-only
   controller creation on 3-button navigation does not skew bottom-chin metrics,
   and check device eligibility before querying `isEdgeToEdgeRefactorEnabled()`
   in `isEdgelessTopInsetSupported()`.
3. **2026-11-06 — Add `kViewportFitCoverAndSafeAreaInsetTop` to
   `ukm_features.cc`**: Approved by `dmurph@chromium.org` because emission is
   bounded by `kViewportFitCover` (`~0.09%` of page loads, well below the `< 1%`
   UKM cap).
4. **2026-11-06 — Skip `TopScalp.PageLoadState` on non-opted-in unsupported
   configs**: Approved by `dmurph@chromium.org` so tablets running
   `EdgeToEdgeControllerImpl` for the bottom chin only record bucket `4`
   (`OPTED_IN_SUPPRESSED_BY_UNSUPPORTED_CONFIG`) when an opted-in page is
   suppressed.

## 6. Plan of Work (Milestones)

### Milestone 1: Blink `kViewportFitCoverAndSafeAreaInsetTop` UseCounter and UKM (`[2/N]`)

- **Concrete Steps:**
  1. Create branch `e2e-uma-cl1` tracking `e2e-uma`.
  2. Add `kViewportFitCoverAndSafeAreaInsetTop = 6097` to
     `third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom`.
  3. Run `vpython3 tools/metrics/histograms/update_use_counter_feature_enum.py`
     to update `tools/metrics/histograms/metadata/blink/enums.xml`.
  4. Add `WebFeature::kViewportFitCoverAndSafeAreaInsetTop` to
     `GetAllowedUkmFeatures()` in
     `components/page_load_metrics/browser/observers/use_counter/ukm_features.cc`.
  5. Update `ViewportData::UpdateViewportDescription()` in
     `third_party/blink/renderer/core/frame/viewport_data.cc` and
     `DocumentStyleEnvironmentVariables::RecordVariableUsage()` in
     `third_party/blink/renderer/core/css/document_style_environment_variables.cc`
     to count `WebFeature::kViewportFitCoverAndSafeAreaInsetTop` when both
     counterpart features are counted on the outermost main frame.
  6. Update `third_party/blink/renderer/core/html/html_meta_element_test.cc` and
     `third_party/blink/renderer/core/css/style_environment_variables_test.cc`.
  7. Run tests:
     `tools/autotest.py -C out/Default --run-all third_party/blink/renderer/core/html/html_meta_element_test.cc third_party/blink/renderer/core/css/style_environment_variables_test.cc`
  8. Run histogram format validation:
     `vpython3 tools/metrics/histograms/validate_format.py`
- **Interfaces and Dependencies:**
  - Extends `blink::mojom::WebFeature` and `GetAllowedUkmFeatures()`.
- **Validation and Acceptance:**
  - `HTMLMetaElementTest.ViewportFit_Cover_IsUseCounted` and
    `StyleEnvironmentVariablesTest.RecordUseCounter_*` pass in `out/Default`.
  - Verify code formatting with: `git cl format`
  - Verify presubmit checks with: `git cl presubmit -u --force`

### Milestone 2: Android Top E2E Startup Eligibility UMA and Synthetic Trial (`[1/N]`)

- **Concrete Steps:**
  1. Create branch `e2e-uma-cl2` tracking `origin/main`.
  2. Add `recordTopEligibilityForEveryStart(Activity)` to
     `chrome/browser/ui/android/edge_to_edge/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeUtils.java`,
     and check device eligibility before `isEdgeToEdgeRefactorEnabled()` in
     `isEdgelessTopInsetSupported(Activity)`.
  3. Update `onFinishNativeInitialization()` and
     `initializeEdgeToEdgeController()` in
     `chrome/android/java/src/org/chromium/chrome/browser/ui/RootUiCoordinator.java`.
  4. Add `Android.EdgeToEdge.Top.Eligible` and
     `Android.EdgeToEdge.Top.IneligibilityReason` to
     `tools/metrics/histograms/metadata/android/histograms.xml`.
  5. Add unit tests in
     `chrome/browser/ui/android/edge_to_edge/internal/junit/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerTest.java`
     covering `recordTopEligibilityForEveryStart`.
  6. Run Robolectric JUnit tests:
     `tools/autotest.py -C out/Android --run-all chrome/browser/ui/android/edge_to_edge/internal/junit/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerTest.java`
  7. Run histogram format validation:
     `vpython3 tools/metrics/histograms/validate_format.py`
- **Interfaces and Dependencies:**
  - Adds `EdgeToEdgeUtils.recordTopEligibilityForEveryStart`.
- **Validation and Acceptance:**
  - `EdgeToEdgeControllerTest` passes in `out/Android`.
  - Verify code formatting with: `git cl format`
  - Verify presubmit checks with: `git cl presubmit -u --force`

### Milestone 3: Android `TopScalp.PageLoadState` UMA and `MobilePageLoadedWithTopToEdge` (`[4/N]`)

- **Concrete Steps:**
  1. Create branch `e2e-uma-cl3` tracking `e2e-uma-cl2`.
  2. Add `recordPageLoadedWithTopToEdge` to
     `chrome/android/java/src/org/chromium/chrome/browser/metrics/UmaSessionStats.java`,
     `chrome/browser/android/metrics/uma_session_stats.cc`, and
     `tools/metrics/actions/actions.xml`.
  3. Add `@IntDef TopScalpPageLoadState` and
     `EdgeToEdgeUtils.recordTopScalpPageLoadState` to
     `chrome/browser/ui/android/edge_to_edge/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeUtils.java`
     with `LINT.IfChange(TopScalpPageLoadState)` guards.
  4. Add `onPageLoadFinished` and `recordTopScalpPageLoadState` (plus shared
     `isPageOptedIntoTopEdgeToEdge`) to
     `chrome/browser/ui/android/edge_to_edge/internal/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerImpl.java`.
  5. Add `Android.EdgeToEdge.TopScalp.PageLoadState` and `TopScalpPageLoadState`
     to `tools/metrics/histograms/metadata/android/histograms.xml` and
     `tools/metrics/histograms/metadata/android/enums.xml`.
  6. Add unit and instrumentation tests in
     `chrome/android/junit/src/org/chromium/chrome/browser/metrics/UmaSessionStatsUnitTest.java`,
     `chrome/browser/ui/android/edge_to_edge/internal/junit/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerTest.java`,
     and
     `chrome/android/javatests/src/org/chromium/chrome/browser/app/edge_to_edge/EdgeToEdgeInstrumentationTest.java`.
  7. Run Robolectric JUnit tests:
     `tools/autotest.py -C out/Android --run-all chrome/android/junit/src/org/chromium/chrome/browser/metrics/UmaSessionStatsUnitTest.java chrome/browser/ui/android/edge_to_edge/internal/junit/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeControllerTest.java`
  8. Compile instrumentation test target:
     `autoninja -C out/Android chrome_test_java`
  9. Run histogram format validation:
     `vpython3 tools/metrics/histograms/validate_format.py`
- **Interfaces and Dependencies:**
  - Adds `UmaSessionStats.Natives#recordPageLoadedWithTopToEdge`,
    `EdgeToEdgeUtils.TopScalpPageLoadState`, and
    `EdgeToEdgeUtils.recordTopScalpPageLoadState`.
  - Keeps `LINT.IfChange(TopScalpPageLoadState)` synchronized between Java and
    XML.
- **Validation and Acceptance:**
  - `UmaSessionStatsUnitTest` and `EdgeToEdgeControllerTest` pass in
    `out/Android`, and `chrome_test_java` compiles cleanly.
  - Verify code formatting with: `git cl format`
  - Verify presubmit checks with: `git cl presubmit -u --force`

### Milestone 4: Verification & Harness Freshness

- **Concrete Steps:**
  1. Format all files with `git cl format`.
  2. Run full target test suites across `out/Default` and `out/Android`.
  3. Run presubmit checks with `git cl presubmit -u --force`.
  4. Run whole-chain `chromium_code_reviewer` audit across stacked branches, and
     evaluate whether the milestone ordering/split is optimal.
  5. **Harness Freshness Audit:** Inspect code changes and verify `AGENTS.md`
     links and guidelines remain accurate.
- **Validation and Acceptance:**
  - All tests green, presubmit passes, chain review approved, and plan updated
    to `status: completed`.

## 7. Artifacts and Notes

- [Parent Design Document](../designs/2026-11-06-top-e2e-metrics-design.md)
- [Edge to Edge Status bar DD (`aixba@`)](https://docs.google.com/document/d/1PE8nIOWk59ODI6iZWF31FaXIbWcCsSf2FjLSLwoVo7Q/edit?tab=t.0#heading=h.54hag4vjo4vo)
- Buganizer: `b/580934118`, `b/567231650`

## 8. Outcomes & Retrospective

All 4 milestones were implemented, verified with unit tests and
`git cl presubmit -u --force`, and approved (`LGTM`) by `chromium_code_reviewer`
across a standalone Blink/UKM CL (`e2e-uma-cl1`) and a 2-CL Android stack
(`e2e-uma-cl2` -> `e2e-uma-cl3`, with `e2e-uma` kept as a local harness branch):

1. **Milestone 1 (`e2e-uma-cl1`, standalone on `origin/main`)**: Added
   `WebFeature::kViewportFitCoverAndSafeAreaInsetTop = 6097` to
   `web_feature.mojom`, `enums.xml`, `ukm_features.cc`, `viewport_data.cc`, and
   `document_style_environment_variables.cc`. Verified with 62 passing tests in
   `blink_unittests` (`HTMLMetaElementTest.*` and
   `StyleEnvironmentVariablesTest.*`).
2. **Milestone 2 (`e2e-uma-cl2`, `[1/N]` on `origin/main`)**: Added
   `Android.EdgeToEdge.Top.Eligible` and
   `Android.EdgeToEdge.Top.IneligibilityReason` histograms, registered the
   symmetric `EdgeToEdgeTopEligibility` synthetic field trial in
   `RootUiCoordinator.onFinishNativeInitialization()`, checked device
   eligibility before `isEdgeToEdgeRefactorEnabled()` in
   `isEdgelessTopInsetSupported()`, and guarded bottom-chin `OnCreateController`
   metrics against top-only (3-button nav) controller startups.
3. **Milestone 3 (`e2e-uma-cl3`, `[2/N]` stacked on `e2e-uma-cl2`)**: Added
   `Android.EdgeToEdge.TopScalp.PageLoadState` (covering all 6 buckets including
   `DISABLED_IN_FULLSCREEN`) and `MobilePageLoadedWithTopToEdge` (gated on
   `!tab.isCustomTab() && isEdgelessTopInsetSupported(activity)`). Verified with
   148 passing tests in `chrome_junit_tests` (`UmaSessionStatsUnitTest` and
   `EdgeToEdgeControllerTest`) and compiled `chrome_test_java`
   (`EdgeToEdgeInstrumentationTest`).
4. **Milestone 4 (Chain Review & Split Evaluation)**: `chromium_design_reviewer`
   and `chromium_code_reviewer` confirmed unstacking `e2e-uma-cl1` onto
   `origin/main` (since Blink/UKM has zero file overlap or `OWNERS` overlap with
   Android) and keeping `e2e-uma-cl2` (`b/567231650`) and `e2e-uma-cl3`
   (`b/580934118`) as a 2-CL Android stack is optimal.
