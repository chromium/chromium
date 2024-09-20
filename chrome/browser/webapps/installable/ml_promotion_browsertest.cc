// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/visited_manifest_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webapps/installable/ml_promotion_browsertest_base.h"
#include "chrome/common/chrome_features.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_install_result_reporter.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace webapps {
namespace {
using InstallUkmEntry = ukm::builders::Site_Install;
using ManifestUkmEntry = ukm::builders::Site_Manifest;
using QualityUkmEntry = ukm::builders::Site_Quality;
using segmentation_platform::HasTrainingLabel;
using segmentation_platform::InputContext;
using segmentation_platform::IsInputContextWithArgs;
using segmentation_platform::MockSegmentationPlatformService;
using segmentation_platform::TrainingRequestId;
using segmentation_platform::processing::ProcessedValue;
using testing::_;
using testing::Pointee;
using webapps::MLInstallabilityPromoter;
using webapps::SiteInstallMetrics;
using webapps::SiteQualityMetrics;
using MlInstallResponse = MlInstallResultReporter::MlInstallResponse;

segmentation_platform::ClassificationResult CreateClassificationResult(
    std::string label,
    TrainingRequestId request_id) {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(label);
  result.request_id = request_id;
  return result;
}

enum class InstallDialogState {
  kSimpleInstallDialog = 0,
  kDetailedInstallDialog = 1,
  kCreateShortcutDialog = 2,
  kMaxValue = kCreateShortcutDialog
};

std::string GetMLPromotionDialogTestName(
    const ::testing::TestParamInfo<InstallDialogState>& info) {
  switch (info.param) {
    case InstallDialogState::kSimpleInstallDialog:
      return "Simple_Install_Dialog";
    case InstallDialogState::kDetailedInstallDialog:
      return "Detailed_Install_Dialog";
    case InstallDialogState::kCreateShortcutDialog:
      return "Create_Shortcut_Dialog";
  }
}

class WebContentsObserverAdapter : public content::WebContentsObserver {
 public:
  explicit WebContentsObserverAdapter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~WebContentsObserverAdapter() override { Observe(nullptr); }

  bool AwaitManifestUrlChanged(GURL expected_manifest_url) {
    expected_manifest_url_ = expected_manifest_url;
    manifest_run_loop_.Run();
    return manifest_url_updated_;
  }

  void AwaitFaviconUrlsChanged() { favicon_run_loop_.Run(); }

  void AwaitVisibilityHidden() {
    if (web_contents()->GetVisibility() == content::Visibility::HIDDEN) {
      return;
    }
    visibility_run_loop_.Run();
  }

 private:
  void DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                               const GURL& manifest_url) override {
    if (expected_manifest_url_ == manifest_url) {
      manifest_url_updated_ = true;
      manifest_run_loop_.Quit();
    }
  }

  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    favicon_run_loop_.Quit();
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (!visibility_run_loop_.running() ||
        visibility != content::Visibility::HIDDEN) {
      return;
    }
    visibility_run_loop_.Quit();
  }

  bool manifest_url_updated_ = false;
  GURL expected_manifest_url_;
  base::RunLoop manifest_run_loop_;

  base::RunLoop favicon_run_loop_;

  base::RunLoop visibility_run_loop_;
};

class MLPromotionBrowserTest : public MLPromotionBrowserTestBase {
 public:
  MLPromotionBrowserTest() {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {base::test::FeatureRefAndParams(
             webapps::features::kWebAppsEnableMLModelForPromotion,
             {{features::kWebAppsMLGuardrailResultReportProb.name, "1.0"},
              {features::kWebAppsMLModelUserDeclineReportProb.name, "1.0"}}),
         base::test::FeatureRefAndParams(
             user_education::features::kUserEducationExperienceVersion2, {})},
        /*disabled_features=*/{});
  }
  ~MLPromotionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    MLPromotionBrowserTestBase::SetUpOnMainThread();
    ml_promoter()->SetTaskRunnerForTesting(task_runner_);
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    // Set the session start time to `features::GetSessionStartGracePeriod()`
    // time ago, to ensure ML install promotion isn't blocked by the grace
    // period.
    SetUserEducationSessionStartTime(
        base::Time::Now() -
        user_education::features::GetSessionStartGracePeriod());
  }

 protected:
  void SetUserEducationSessionStartTime(base::Time time) {
    UserEducationService* edu_service =
        UserEducationServiceFactory::GetForBrowserContext(profile());
    user_education::FeaturePromoSessionData session_data;
    session_data.start_time = time;
    session_data.most_recent_active_time = base::Time::Now();
    edu_service->feature_promo_storage_service()
        .set_profile_creation_time_for_testing(time);
    edu_service->feature_promo_storage_service().SaveSessionData(session_data);
  }

  GURL GetUrlWithFaviconsNoManifest() {
    return https_server()->GetURL("/banners/test_page_with_favicon.html");
  }

  GURL GetInstallableAppURL() {
    return https_server()->GetURL("/banners/manifest_test_page.html");
  }

  GURL GetUrlWithNoManifest() {
    return https_server()->GetURL("/banners/no_manifest_test_page.html");
  }

  GURL GetManifestUrlForNoManifestTestPage() {
    return https_server()->GetURL(
        "/banners/manifest_for_no_manifest_page.json");
  }

  GURL GetUrlWithManifestAllFieldsLoadedForML() {
    return https_server()->GetURL("/banners/test_page_for_ml_promotion.html");
  }

  GURL GetUrlWithNoSWNoFetchHandler() {
    return https_server()->GetURL("/banners/manifest_no_service_worker.html");
  }

  GURL GetUrlWithSWEmptyFetchHandler() {
    return https_server()->GetURL(
        "/banners/manifest_test_page_empty_fetch_handler.html");
  }

  GURL GetUrlWithSwNoFetchHandler() {
    return https_server()->GetURL(
        "/banners/no_sw_fetch_handler_test_page.html");
  }

  GURL GetUrlOuterApp() {
    return https_server()->GetURL("/web_apps/nesting/index.html");
  }

  GURL GetUrlInnerCraftedApp() {
    return https_server()->GetURL("/web_apps/nesting/nested/index.html");
  }

  GURL GetUrlInnerDiyApp() {
    return https_server()->GetURL("/web_apps/nesting/nested/diy.html");
  }

  MLInstallabilityPromoter* ml_promoter() {
    return MLInstallabilityPromoter::FromWebContents(web_contents());
  }

  web_app::WebAppProvider& provider() {
    // TODO(b/287255120) : Block this on Android.
    return *web_app::WebAppProvider::GetForTest(profile());
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return *test_ukm_recorder_;
  }

  // Wait for service workers to be registered and the MLInstallabilityPromoter
  // to finish.
  void NavigateAwaitSWRegistrationPendingDelayedTask(const GURL& url) {
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(profile(),
                                                                 url);
    NavigateAndAwaitInstallabilityCheck(url);
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
    registration_waiter.AwaitRegistrationStored();
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
  }

  void NavigateUpdateManifestAndAwaitDelayedTaskPending(
      const GURL& site_url,
      const GURL& new_manifest_url) {
    NavigateAndAwaitMetricsCollectionPending(site_url);
    WebContentsObserverAdapter web_contents_observer(web_contents());
    EXPECT_TRUE(content::ExecJs(
        web_contents(),
        "addManifestLinkTag('/banners/manifest_for_no_manifest_page.json')"));
    EXPECT_TRUE(
        web_contents_observer.AwaitManifestUrlChanged(new_manifest_url));
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
  }

  void UpdateFaviconAndRunPipelinePendingDelayedTask(const GURL& url) {
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
    WebContentsObserverAdapter web_contents_observer(web_contents());
    NavigateAndAwaitInstallabilityCheck(url);
    EXPECT_TRUE(content::ExecJs(web_contents(), "addFavicon('favicon_1.ico')"));
    web_contents_observer.AwaitFaviconUrlsChanged();
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
  }

  void NavigateAndAwaitMetricsCollectionPending(const GURL& url) {
    NavigateAndAwaitInstallabilityCheck(url);
    ml_promoter()->AwaitMetricsCollectionTasksCompleteForTesting();
  }

  void ExpectClasificationCallReturnResult(
      GURL site_url,
      webapps::ManifestId manifest_id,
      std::string label_result,
      TrainingRequestId request_result,
      content::WebContents* custom_web_contents = nullptr,
      int times_called = 1) {
    if (!custom_web_contents) {
      custom_web_contents = web_contents();
    }
    base::flat_map<std::string, ProcessedValue> expected_input = {
        {"origin", ProcessedValue(url::Origin::Create(site_url).GetURL())},
        {"site_url", ProcessedValue(site_url)},
        {"manifest_id", ProcessedValue(manifest_id)}};
    EXPECT_CALL(*GetMockSegmentation(),
                GetClassificationResult(
                    segmentation_platform::kWebAppInstallationPromoKey, _,
                    Pointee(testing::Field(&InputContext::metadata_args,
                                           testing::Eq(expected_input))),
                    _))
        .Times(testing::Exactly(times_called))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<3>(
            CreateClassificationResult(label_result, request_result)));
  }

  void ExpectTrainingResult(
      TrainingRequestId request,
      MlInstallResponse response,
      content::WebContents* custom_web_contents = nullptr) {
    if (!custom_web_contents) {
      custom_web_contents = web_contents();
    }
    EXPECT_CALL(*GetMockSegmentation(),
                CollectTrainingData(
                    segmentation_platform::proto::SegmentId::
                        OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO,
                    request,
                    HasTrainingLabel(
                        "WebApps.MlInstall.DialogResponse",
                        static_cast<base::HistogramBase::Sample>(response)),
                    _));
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

// Manifest Data Fetching tests.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, CompletelyFilledManifestUKM) {
  NavigateAndAwaitMetricsCollectionPending(
      GetUrlWithManifestAllFieldsLoadedForML());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(ManifestUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(
      entry, GetUrlWithManifestAllFieldsLoadedForML());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kDisplayModeName, /*browser=*/1);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasBackgroundColorName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasNameName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsAnyName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsMaskableName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasScreenshotsName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasThemeColorName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasStartUrlName, /*kValid=*/2);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, PartiallyFilledManifestUKM) {
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(ManifestUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kDisplayModeName, /*standalone=*/3);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasBackgroundColorName, false);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasNameName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsAnyName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsMaskableName, false);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasScreenshotsName, false);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasThemeColorName, false);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasStartUrlName, /*kValid=*/2);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, NoManifestUKM) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(ManifestUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GetUrlWithNoManifest());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kDisplayModeName, -1);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasBackgroundColorName,
      /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry,
                                                 ManifestUkmEntry::kHasNameName,
                                                 /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsAnyName,
      /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasIconsMaskableName,
      /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasScreenshotsName,
      /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasThemeColorName,
      /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasStartUrlName, -1);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, ManifestUpdateChangesUKM) {
  // Run the pipeline with the first update, verify no manifest data is logged
  // to UKMs.
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(ManifestUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  // Verify UKM records empty manifest data.
  test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GetUrlWithNoManifest());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kDisplayModeName, -1);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry,
                                                 ManifestUkmEntry::kHasNameName,
                                                 /*NullableBoolean::Null=*/2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, ManifestUkmEntry::kHasStartUrlName, -1);

  // Restart the pipeline by navigating to about::blank and then navigating back
  // to the no manifest page.
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));

  NavigateUpdateManifestAndAwaitDelayedTaskPending(
      GetUrlWithNoManifest(), GetManifestUrlForNoManifestTestPage());
  task_runner_->RunPendingTasks();

  auto updated_entries =
      test_ukm_recorder().GetEntriesByName(ManifestUkmEntry::kEntryName);
  ASSERT_EQ(updated_entries.size(), 2u);
  auto* updated_entry = updated_entries[1].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(updated_entry,
                                              GetUrlWithNoManifest());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      updated_entry, ManifestUkmEntry::kDisplayModeName, /*kStandAlone=*/3);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      updated_entry, ManifestUkmEntry::kHasNameName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      updated_entry, ManifestUkmEntry::kHasStartUrlName, /*kValid=*/2);
}

// SiteInstallMetrics tests.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, FullyInstalledAppMeasurement) {
  NavigateAndAwaitInstallabilityCheck(GetInstallableAppURL());
  EXPECT_TRUE(InstallAppForCurrentWebContents(/*install_locally=*/true));

  NavigateAndAwaitInstallabilityCheck(GetUrlWithNoManifest());

  // A re-navigation should retrigger the ML pipeline.
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(InstallUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, InstallUkmEntry::kIsFullyInstalledName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, InstallUkmEntry::kIsPartiallyInstalledName, false);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       PartiallyInstalledAppMeasurement) {
  NavigateAndAwaitInstallabilityCheck(GetInstallableAppURL());
  EXPECT_TRUE(InstallAppForCurrentWebContents(/*install_locally=*/false));

  NavigateAndAwaitInstallabilityCheck(GetUrlWithNoManifest());
  // A re-navigation should retrigger the ML pipeline.
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  auto entries =
      test_ukm_recorder().GetEntriesByName(InstallUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GetInstallableAppURL());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, InstallUkmEntry::kIsFullyInstalledName, false);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, InstallUkmEntry::kIsPartiallyInstalledName, true);
}

// SiteQualityMetrics tests.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       SiteQualityMetrics_ServiceWorker_FetchHandler) {
  NavigateAwaitSWRegistrationPendingDelayedTask(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName,
      {QualityUkmEntry::kHasFetchHandlerName,
       QualityUkmEntry::kServiceWorkerScriptSizeName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetInstallableAppURL());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFetchHandlerName], /*true=*/1);
  EXPECT_GT(entry.metrics[QualityUkmEntry::kServiceWorkerScriptSizeName], 0);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       SiteQualityMetrics_NoServiceWorker_NoFetchHandler) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoSWNoFetchHandler());
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName,
      {QualityUkmEntry::kHasFetchHandlerName,
       QualityUkmEntry::kServiceWorkerScriptSizeName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithNoSWNoFetchHandler());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFetchHandlerName], /*false=*/0);
  // Non-existence of a service worker is shown by a script size of 0.
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kServiceWorkerScriptSizeName], 0);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       SiteQualityMetrics_ServiceWorker_EmptyFetchHandler) {
  NavigateAwaitSWRegistrationPendingDelayedTask(
      GetUrlWithSWEmptyFetchHandler());
  task_runner_->RunPendingTasks();

  // An empty fetch handler is also treated as an existence of a fetch handler.
  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName,
      {QualityUkmEntry::kHasFetchHandlerName,
       QualityUkmEntry::kServiceWorkerScriptSizeName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithSWEmptyFetchHandler());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFetchHandlerName], /*true=*/1);
  EXPECT_GT(entry.metrics[QualityUkmEntry::kServiceWorkerScriptSizeName], 0);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       SiteQualityMetrics_ServiceWorker_NoFetchHandler) {
  NavigateAwaitSWRegistrationPendingDelayedTask(GetUrlWithSwNoFetchHandler());
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName,
      {QualityUkmEntry::kHasFetchHandlerName,
       QualityUkmEntry::kServiceWorkerScriptSizeName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithSwNoFetchHandler());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFetchHandlerName], /*false=*/0);
  EXPECT_GT(entry.metrics[QualityUkmEntry::kServiceWorkerScriptSizeName], 0);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       PageLoadsWithDefaultFaviconName) {
  GURL url_with_default_favicon_name = https_server()->GetURL(
      "/banners/test_page_with_default_favicon_name.html");
  NavigateAndAwaitMetricsCollectionPending(url_with_default_favicon_name);
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName, {QualityUkmEntry::kHasFaviconsName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            url_with_default_favicon_name);
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFaviconsName], 1);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, PageLoadsWithExistingFavicon) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithFaviconsNoManifest());
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName, {QualityUkmEntry::kHasFaviconsName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithFaviconsNoManifest());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFaviconsName], 1);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, PageLoadVerifyFaviconUpdate) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());
  task_runner_->RunPendingTasks();

  auto entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName, {QualityUkmEntry::kHasFaviconsName});
  ASSERT_EQ(entries.size(), 1u);

  auto entry = entries[0];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithNoManifest());
  EXPECT_EQ(entry.metrics[QualityUkmEntry::kHasFaviconsName], 0);

  // Navigate to a different page to reset the site URL the pipeline needs to be
  // triggered from.
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));

  // Add favicons to page after loading and trigger pipeline for testing.
  UpdateFaviconAndRunPipelinePendingDelayedTask(GetUrlWithNoManifest());

  task_runner_->RunPendingTasks();

  auto updated_entries = test_ukm_recorder().GetEntries(
      QualityUkmEntry::kEntryName, {QualityUkmEntry::kHasFaviconsName});
  EXPECT_EQ(updated_entries.size(), 2u);

  auto updated_entry = updated_entries[1];
  EXPECT_EQ(test_ukm_recorder().GetSourceForSourceId(entry.source_id)->url(),
            GetUrlWithNoManifest());
  EXPECT_EQ(updated_entry.metrics[QualityUkmEntry::kHasFaviconsName], 1);
}

// This test is not parameterized because the site has no manifest (hence no
// screenshots), hence only the single bubble view should show up.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       SitePassingGuardrailsNoManifestDoesNotCrash) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithFaviconsNoManifest());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlWithFaviconsNoManifest(),
      /*manifest_id=*/GetUrlWithFaviconsNoManifest(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  // Since the site is not installable, the diy install dialog shows up for
  // universal install.
  std::string bubble_name_to_use =
      base::FeatureList::IsEnabled(::features::kWebAppUniversalInstall)
          ? "WebAppDiyInstallDialog"
          : "PWAConfirmationBubbleView";
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       bubble_name_to_use);
  task_runner_->RunPendingTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kAccepted);
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(provider().registrar_unsafe().is_empty());
  webapps::AppId app_id = provider().registrar_unsafe().GetAppIds()[0];
  EXPECT_EQ("Web App Test Page with Favicon",
            provider().registrar_unsafe().GetAppShortName(app_id));
  auto user_display_mode =
      provider().registrar_unsafe().GetAppUserDisplayMode(app_id);
  EXPECT_TRUE(user_display_mode.has_value());
  EXPECT_THAT(user_display_mode.value(),
              web_app::mojom::UserDisplayMode::kStandalone);
}

// The fact that this test does not crash is proof that the guardrails based
// check works for an empty site (no manifest and no icons).
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest, MLInstallEmptyPageNoIcons) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlWithNoManifest(),
      /*manifest_id=*/GetUrlWithNoManifest(),
      MLInstallabilityPromoter::kShowInstallPromptLabel, TrainingRequestId(1ll),
      web_contents());
  task_runner_->RunPendingTasks();

  ExpectTrainingResult(TrainingRequestId(1ll),
                       MlInstallResponse::kBlockedGuardrails);
  // Doing another navigation should now trigger the guardrail blocked signal.
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));
}

// Test for crbug.com/1472629, where an already open dialog would cause
// the CHECK in MLInstallOperationTracker::OnMlResultForInstallation
// to fail, since the operation tracker would outlive the results of the
// pipeline.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTest,
                       MLPipelineNoCrashForExistingTracker) {
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());

  bool universal_install_enabled =
      base::FeatureList::IsEnabled(::features::kWebAppUniversalInstall);

  if (universal_install_enabled) {
    // Expect the pipeline to trigger both on the first and second url.
    ExpectClasificationCallReturnResult(
        /*site_url=*/GetInstallableAppURL(),
        /*manifest_id=*/GetInstallableAppURL(),
        MLInstallabilityPromoter::kShowInstallPromptLabel,
        TrainingRequestId(1ll), web_contents());
    ExpectClasificationCallReturnResult(
        /*site_url=*/GetUrlOuterApp(),
        /*manifest_id=*/GetUrlOuterApp(),
        MLInstallabilityPromoter::kShowInstallPromptLabel,
        TrainingRequestId(2ll), web_contents());

  } else {
    // This assertion is still needed on CI trybots that do not enable the field
    // trial configs.
    ExpectClasificationCallReturnResult(
        /*site_url=*/GetInstallableAppURL(),
        /*manifest_id=*/GetInstallableAppURL(),
        MLInstallabilityPromoter::kShowInstallPromptLabel,
        TrainingRequestId(1ll), web_contents());
    ExpectClasificationCallReturnResult(
        /*site_url=*/GetUrlOuterApp(),
        /*manifest_id=*/GetUrlOuterApp(),
        MLInstallabilityPromoter::kShowInstallPromptLabel,
        TrainingRequestId(2ll), web_contents());
  }

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       universal_install_enabled
                                           ? "WebAppSimpleInstallDialog"
                                           : "PWAConfirmationBubbleView");
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(widget != nullptr);

  task_runner_->RunPendingTasks();

  // Refreshing the page should exit the pipeline early, and should not crash.
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrlOuterApp());
  task_runner_->RunPendingTasks();
}

class MLPromotionBrowserTestNestedPromptBlocking
    : public MLPromotionBrowserTest {
 public:
  MLPromotionBrowserTestNestedPromptBlocking() = default;

  base::test::ScopedFeatureList enable_feature_{
      web_app::kBlockMlPromotionInNestedPagesNoManifest};
};

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTestNestedPromptBlocking,
                       NoPromptForNestedDiy) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlOuterApp());

  // Wait for the full ml pipeline to run. The reporting of the manifest from
  // the AppBannerManager to the VisitedManifestManager should happen by the
  // time this finishes.
  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlOuterApp(),
      /*manifest_id=*/GetUrlOuterApp(),
      MLInstallabilityPromoter::kDontShowLabel, TrainingRequestId(1ll),
      web_contents());
  task_runner_->RunPendingTasks();
  ExpectTrainingResult(TrainingRequestId(1ll),
                       MlInstallResponse::kReporterDestroyed);

  // Navigate now to the DIY app (no manifest) to ensure that it is blocked.
  NavigateAndAwaitMetricsCollectionPending(GetUrlInnerDiyApp());

  task_runner_->RunPendingTasks();
  base::test::RunUntil(
      [this]() { return ml_promoter()->IsCompleteForTesting(); });
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowserTestNestedPromptBlocking,
                       PromptForNestedCraftedApp) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlOuterApp());

  // Wait for the full ml pipeline to run. The reporting of the manifest from
  // the AppBannerManager to the VisitedManifestManager should happen by the
  // time this finishes.
  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlOuterApp(),
      /*manifest_id=*/GetUrlOuterApp(),
      MLInstallabilityPromoter::kDontShowLabel, TrainingRequestId(1ll),
      web_contents());
  task_runner_->RunPendingTasks();
  ExpectTrainingResult(TrainingRequestId(1ll),
                       MlInstallResponse::kReporterDestroyed);

  // Navigate now to the crafted app to ensure that it is not blocked, and test
  // installation.
  NavigateAndAwaitMetricsCollectionPending(GetUrlInnerCraftedApp());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlInnerCraftedApp(),
      /*manifest_id=*/GetUrlInnerCraftedApp(),
      MLInstallabilityPromoter::kShowInstallPromptLabel, TrainingRequestId(2ll),
      web_contents());

  std::string bubble_name_to_use =
      base::FeatureList::IsEnabled(::features::kWebAppUniversalInstall)
          ? "WebAppSimpleInstallDialog"
          : "PWAConfirmationBubbleView";
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       bubble_name_to_use);
  task_runner_->RunPendingTasks();
  ExpectTrainingResult(TrainingRequestId(2ll), MlInstallResponse::kAccepted);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  ASSERT_FALSE(provider().registrar_unsafe().is_empty());
  webapps::AppId app_id = provider().registrar_unsafe().GetAppIds()[0];
  EXPECT_EQ(GetUrlInnerCraftedApp(),
            provider().registrar_unsafe().GetAppStartUrl(app_id));
}

// TODO(b/285361272): Add tests for cache storage sizes.
// TODO(b/329696741): Update DIY App dialog information also here.
class MLPromotionInstallDialogBrowserTest
    : public MLPromotionBrowserTest,
      public ::testing::WithParamInterface<InstallDialogState> {
 public:
  MLPromotionInstallDialogBrowserTest() = default;
  ~MLPromotionInstallDialogBrowserTest() = default;

 protected:
  const std::string GetDialogName() {
    switch (GetParam()) {
      case InstallDialogState::kSimpleInstallDialog:
        return GetSimpleInstallDialogNameBasedOnUniversalInstall();
      case InstallDialogState::kDetailedInstallDialog:
        return "WebAppDetailedInstallDialog";
      case InstallDialogState::kCreateShortcutDialog:
        return "CreateShortcutConfirmationView";
    }
  }

  const GURL GetUrlBasedOnDialogState() {
    switch (GetParam()) {
      case InstallDialogState::kSimpleInstallDialog:
        return GetInstallableAppURL();
      case InstallDialogState::kDetailedInstallDialog:
        return https_server()->GetURL(
            "/banners/manifest_test_page_screenshots.html");
      case InstallDialogState::kCreateShortcutDialog:
        return GetUrlWithNoManifest();
    }
  }

  // These names are obtained from the manifests in chrome/test/data/banners/
  const std::string GetAppNameBasedOnDialogState() {
    switch (GetParam()) {
      case InstallDialogState::kSimpleInstallDialog:
        return "Manifest test app";
      case InstallDialogState::kDetailedInstallDialog:
        return "PWA Bottom Sheet";
      case InstallDialogState::kCreateShortcutDialog:
        NOTREACHED_IN_MIGRATION();
        return std::string();
    }
  }

  void InstallAppBasedOnDialogState() {
    switch (GetParam()) {
      case InstallDialogState::kSimpleInstallDialog:
      case InstallDialogState::kDetailedInstallDialog:
        InstallAppForCurrentWebContents(/*install_locally=*/true);
        break;
      case InstallDialogState::kCreateShortcutDialog:
        web_app::SetAutoAcceptWebAppDialogForTesting(
            /*auto_accept=*/true, /*auto_open_in_window=*/false);
        chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT);
        break;
    }
  }

  bool IsCurrentTestStateShortcutDialog() {
    return GetParam() == InstallDialogState::kCreateShortcutDialog;
  }

 private:
  const std::string GetSimpleInstallDialogNameBasedOnUniversalInstall() {
    if (base::FeatureList::IsEnabled(::features::kWebAppUniversalInstall)) {
      return "WebAppSimpleInstallDialog";
    }
    return "PWAConfirmationBubbleView";
  }
};

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest, MlInstallNotShown) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(), "DontShow",
      TrainingRequestId(1ll));

  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().is_empty());
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallShownCancelled) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();

  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kCancelled);

  views::test::CancelDialog(widget);

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().is_empty());
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallShownIgnoredNavigation) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();

  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kIgnored);

  views::test::WidgetDestroyedWaiter destroyed(widget);
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));
  destroyed.Wait();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().is_empty());
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallShownIgnoredWidgetClosed) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kIgnored);
  widget->Close();
  destroyed.Wait();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_TRUE(provider().registrar_unsafe().is_empty());
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallShownAccepted) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kAccepted);
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(provider().registrar_unsafe().is_empty());
  webapps::AppId app_id = provider().registrar_unsafe().GetAppIds()[0];
  EXPECT_EQ(GetAppNameBasedOnDialogState(),
            provider().registrar_unsafe().GetAppShortName(app_id));
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlNotShownAlreadyInstalled) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());
  InstallAppBasedOnDialogState();

  int segmentation_trigger_time =
      (GetParam() == InstallDialogState::kCreateShortcutDialog);
  // ML Model is still triggered for shortcuts which are treated separately as
  // PWAs.
  EXPECT_CALL(*GetMockSegmentation(), GetClassificationResult(_, _, _, _))
      .Times(segmentation_trigger_time);

  // This calls unblocks the metrics tasks, allowing ML to be called. It should
  // not, though, as the app is installed.
  task_runner_->RunPendingTasks();

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlHandlesInvisible) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  MLInstallabilityPromoter* original_tab_promoter = ml_promoter();
  content::WebContents* original_web_contents = web_contents();

  // Creating a new tab should ensure that visibility changes.
  WebContentsObserverAdapter hidden_waiter(original_web_contents);
  chrome::NewTab(browser());
  hidden_waiter.AwaitVisibilityHidden();

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel, TrainingRequestId(1ll),
      original_web_contents);

  // This calls unblocks the metrics tasks, allowing ML to be called. However,
  // because the web contents isn't visible, the results won't be reported yet.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(original_tab_promoter->IsPendingVisibilityForTesting());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().is_empty());

  // Navigating to the previous tab will resume the installation UX reporting,
  // so handle installation request.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kAccepted,
                       original_web_contents);
  chrome::SelectPreviousTab(browser());
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  views::test::AcceptDialog(widget);
  destroyed.Wait();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(provider().registrar_unsafe().is_empty());
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallBlockedIphGracePeriod) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  // Setting the start time to now should apply the grace period, blocking ml
  // install promotion via guardrail.
  SetUserEducationSessionStartTime(base::Time::Now());

  // This calls unblocks the metrics tasks, allowing the pipeline to continue
  // and check guardrails.
  task_runner_->RunPendingTasks();

  // Ensure that nothing is installed.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().is_empty());

  ExpectTrainingResult(TrainingRequestId(1ll),
                       MlInstallResponse::kBlockedGuardrails, web_contents());
  // Doing another navigation should now trigger the guardrail blocked signal.
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallGuardrailBlocked) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       GetDialogName());
  // This calls unblocks the metrics tasks, allowing ML to be called.
  task_runner_->RunPendingTasks();

  // Cancelling the dialog will save that result in the guardrails, which should
  // cause the next immediate install call to trigger the guardrail response.
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kCancelled);
  views::test::WidgetDestroyedWaiter destroyed(widget);
  views::test::CancelDialog(widget);
  destroyed.Wait();

  // Ensure that nothing is installed.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().is_empty());

  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));

  // Test that guardrails now block the install.
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(2ll));
  // This will cause the ML pipeline to complete, but not report anything yet.
  // This allows the user install to possibly still happen and report success.
  task_runner_->RunPendingTasks();

  ExpectTrainingResult(TrainingRequestId(2ll),
                       MlInstallResponse::kBlockedGuardrails, web_contents());
  // Doing another navigation should now trigger the guardrail blocked signal.
  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallGuardrailIgnoredUserInstallAccepted) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  // Cancelling the dialog will save that result in the guardrails, which
  // should cause the next immediate install call to trigger the guardrail
  // response. This is not triggered for the create shortcut dialog since that
  // flow is not shown here.
  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         GetDialogName());
    // This calls unblocks the metrics tasks, allowing ML to be called.
    task_runner_->RunPendingTasks();

    views::Widget* widget = waiter.WaitIfNeededAndGet();
    ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kCancelled);
    views::test::WidgetDestroyedWaiter destroyed(widget);
    views::test::CancelDialog(widget);
    destroyed.Wait();
  }
  // Ensure that nothing is installed.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().is_empty());

  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));

  // Navigate back to the app url to re-trigger the ml pipeline.
  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(2ll));
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());
  task_runner_->RunPendingTasks();

  // Test that the guardrail isn't reported when the user completes the install,
  // and instead reports success.
  ExpectTrainingResult(TrainingRequestId(2ll), MlInstallResponse::kAccepted);
  EXPECT_TRUE(
      InstallAppFromUserInitiation(/*accept_install=*/true, GetDialogName()));
}

IN_PROC_BROWSER_TEST_P(MLPromotionInstallDialogBrowserTest,
                       MlInstallGuardrailIgnoredUserInstallCancelled) {
  if (IsCurrentTestStateShortcutDialog()) {
    GTEST_SKIP()
        << "Skipping because ML cannot trigger the Create Shortcut Dialog.";
  }
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());

  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(1ll));

  // Cancelling the dialog will save that result in the guardrails, which
  // should cause the next immediate install call to trigger the guardrail
  // response.
  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         GetDialogName());
    // This calls unblocks the metrics tasks, allowing ML to be called.
    task_runner_->RunPendingTasks();

    views::Widget* widget = waiter.WaitIfNeededAndGet();
    ExpectTrainingResult(TrainingRequestId(1ll), MlInstallResponse::kCancelled);
    views::test::WidgetDestroyedWaiter destroyed(widget);
    views::test::CancelDialog(widget);
    destroyed.Wait();
  }
  // Ensure that nothing is installed.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().is_empty());

  web_app::NavigateViaLinkClickToURLAndWait(browser(),
                                            GURL(url::kAboutBlankURL));

  // Navigate back to the app url to re-trigger the ml pipeline.
  ExpectClasificationCallReturnResult(
      /*site_url=*/GetUrlBasedOnDialogState(),
      /*manifest_id=*/GetUrlBasedOnDialogState(),
      MLInstallabilityPromoter::kShowInstallPromptLabel,
      TrainingRequestId(2ll));
  NavigateAndAwaitMetricsCollectionPending(GetUrlBasedOnDialogState());
  task_runner_->RunPendingTasks();

  // Test that the guardrail isn't reported when the user completes the install,
  // and instead reports success.
  ExpectTrainingResult(TrainingRequestId(2ll), MlInstallResponse::kCancelled);
  EXPECT_TRUE(
      InstallAppFromUserInitiation(/*accept_install=*/false, GetDialogName()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MLPromotionInstallDialogBrowserTest,
    ::testing::Values(InstallDialogState::kSimpleInstallDialog,
                      InstallDialogState::kDetailedInstallDialog,
                      InstallDialogState::kCreateShortcutDialog),
    GetMLPromotionDialogTestName);

}  // namespace
}  // namespace webapps
