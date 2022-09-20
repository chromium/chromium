// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/accuracy_tips/accuracy_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/features.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/test/widget_test_api.h"

using ::testing::_;

namespace {
using accuracy_tips::AccuracyTipInteraction;
using accuracy_tips::AccuracyTipStatus;

const char kAccuracyTipUrl[] = "a.test";
const char kRegularUrl[] = "b.test";

bool IsUIShowing() {
  return PageInfoBubbleViewBase::BUBBLE_ACCURACY_TIP ==
         PageInfoBubbleViewBase::GetShownBubbleType();
}

}  // namespace

class AccuracyTipBubbleViewBrowserTest : public InProcessBrowserTest {
 protected:
  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    SetUpFeatureList();

    // Disable "close on deactivation" since there seems to be an issue with
    // windows losing focus during tests.
    views::DisableActivationChangeHandlingForTests();

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void ClickExtraButton() {
    auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
    views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
    auto* button = static_cast<views::Button*>(view->GetExtraView());
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
    waiter.Wait();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  virtual void SetUpFeatureList() {
    const base::FieldTrialParams accuracy_tips_params = {
        {accuracy_tips::features::kSampleUrl.name,
         GetUrl(kAccuracyTipUrl).spec()},
        {accuracy_tips::features::kNumIgnorePrompts.name, "1"}};
    const base::FieldTrialParams accuracy_survey_params = {
        {accuracy_tips::features::kMinPromptCountRequiredForSurvey.name, "2"},
        {"probability", "1.000"}};
    feature_list_.InitWithFeaturesAndParameters(
        {{safe_browsing::kAccuracyTipsFeature, accuracy_tips_params},
         {accuracy_tips::features::kAccuracyTipsSurveyFeature,
          accuracy_survey_params}},
        {});
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, NoShowOnRegularUrl) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kRegularUrl)));
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                                         AccuracyTipStatus::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, ShowOnUrlInList) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.PageStatus", AccuracyTipStatus::kShowAccuracyTip, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest,
                       DontShowOnUrlInListWithEngagement) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url = GetUrl(kAccuracyTipUrl);
  auto* engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(
          browser()->profile());
  engagement_service->AddPointsForTesting(
      GetUrl(kAccuracyTipUrl),
      site_engagement::SiteEngagementScore::GetMediumEngagementBoundary());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.PageStatus", AccuracyTipStatus::kHighEnagagement, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AccuracyTipStatus::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AccuracyTipStatus::kStatusName,
      static_cast<int>(AccuracyTipStatus::kHighEnagagement));
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, PressIgnoreButton) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url = GetUrl(kAccuracyTipUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(IsUIShowing());

  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->CancelDialog();
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kClosed, 1);

  auto status_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AccuracyTipStatus::kEntryName);
  EXPECT_EQ(1u, status_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(status_entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      status_entries[0], ukm::builders::AccuracyTipStatus::kStatusName,
      static_cast<int>(AccuracyTipStatus::kShowAccuracyTip));

  auto interaction_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AccuracyTipDialog::kEntryName);
  EXPECT_EQ(1u, interaction_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[0],
      ukm::builders::AccuracyTipDialog::kInteractionName,
      static_cast<int>(AccuracyTipInteraction::kClosed));
}

// TODO(crbug.com/1363619): Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, DISABLED_PressEsc) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  EXPECT_TRUE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_EQ(location_bar_view->location_icon_view()->GetText(),
            l10n_util::GetStringUTF16(IDS_ACCURACY_CHECK_VERBOSE_STATE));

  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  // Simulate esc key pressed.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  view->GetWidget()->OnKeyEvent(&key_event);
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  EXPECT_FALSE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_TRUE(location_bar_view->location_icon_view()->GetText().empty());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kClosed, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, OptOut) {
  auto* service = AccuracyServiceFactory::GetForProfile(browser()->profile());
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  service->SetClockForTesting(&clock);

  // The first time the dialog is shown, it has an "ignore" button.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());
  ClickExtraButton();
  EXPECT_FALSE(IsUIShowing());
  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kIgnore, 1);

  // The ui won't show again on an immediate navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_FALSE(IsUIShowing());

  // But a week later it shows up again with an opt-out button.
  clock.Advance(base::Days(7));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());
  ClickExtraButton();
  EXPECT_FALSE(IsUIShowing());
  histogram_tester()->ExpectTotalCount(
      "Privacy.AccuracyTip.AccuracyTipInteraction", 2);
  histogram_tester()->ExpectBucketCount(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kOptOut, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, DisappearOnNavigate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  // Tip disappears when navigating somewhere else.
  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kRegularUrl)));
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kNoAction, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest,
                       CloseBrowserWhileTipIsOpened) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest,
                       DisappearAfterPermissionRequested) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  // Tip disappears when the site requested permission.
  auto test_api =
      std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  EXPECT_TRUE(test_api->manager());
  test_api->AddSimpleRequest(browser()
                                 ->tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetPrimaryMainFrame(),
                             permissions::RequestType::kGeolocation);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kPermissionRequested, 1);
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest, OpenLearnMoreLink) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());

  // Click "learn more" and expect help center to open.
  auto* view = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  content::WebContentsAddedObserver new_tab_observer;
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->AcceptDialog();
  EXPECT_EQ(GURL(chrome::kSafetyTipHelpCenterURL),
            new_tab_observer.GetWebContents()->GetVisibleURL());
  waiter.Wait();
  EXPECT_FALSE(IsUIShowing());

  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.AccuracyTipInteraction",
      AccuracyTipInteraction::kLearnMore, 1);
}

// Test is flaky on MSAN. https://crbug.com/1241933
#if defined(MEMORY_SANITIZER)
#define MAYBE_SurveyShownAfterShowingTip DISABLED_SurveyShownAfterShowingTip
#else
#define MAYBE_SurveyShownAfterShowingTip SurveyShownAfterShowingTip
#endif
IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest,
                       MAYBE_SurveyShownAfterShowingTip) {
  auto* tips_service =
      AccuracyServiceFactory::GetForProfile(browser()->profile());
  auto* hats_service =
      HatsServiceFactory::GetForProfile(browser()->profile(), true);

  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  tips_service->SetClockForTesting(&clock);

  for (int i = 0;
       i < accuracy_tips::features::kMinPromptCountRequiredForSurvey.Get();
       i++) {
    clock.Advance(base::Days(7));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
    EXPECT_TRUE(IsUIShowing());

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Enable metrics and set the profile creation time to be old enough to ensure
  // the survey triggering.
  bool enable_metrics = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &enable_metrics);
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(45));

  clock.Advance(accuracy_tips::features::kMinTimeToShowSurvey.Get());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(hats_service->hats_next_dialog_exists_for_testing());
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewBrowserTest,
                       SurveyNotShownAfterDeletingHistory) {
  auto* tips_service =
      AccuracyServiceFactory::GetForProfile(browser()->profile());
  auto* hats_service =
      HatsServiceFactory::GetForProfile(browser()->profile(), true);

  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  tips_service->SetClockForTesting(&clock);

  for (int i = 0;
       i < accuracy_tips::features::kMinPromptCountRequiredForSurvey.Get();
       i++) {
    clock.Advance(base::Days(7));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
    EXPECT_TRUE(IsUIShowing());

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Enable metrics and set the profile creation time to be old enough to ensure
  // the survey triggering.
  bool enable_metrics = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &enable_metrics);
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(45));

  // Delete all history...
  tips_service->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());

  clock.Advance(accuracy_tips::features::kMinTimeToShowSurvey.Get());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  base::RunLoop().RunUntilIdle();
  // ...and because of that, a survey won't be shown.
  EXPECT_FALSE(hats_service->hats_next_dialog_exists_for_testing());
}

class AccuracyTipBubbleViewHttpBrowserTest
    : public AccuracyTipBubbleViewBrowserTest {
 public:
  GURL GetHttpUrl(const std::string& host) {
    return embedded_test_server()->GetURL(host, "/title1.html");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    AccuracyTipBubbleViewBrowserTest::SetUp();
  }

 private:
  void SetUpFeatureList() override {
    const base::FieldTrialParams accuraty_tips_params = {
        {accuracy_tips::features::kSampleUrl.name,
         GetHttpUrl(kAccuracyTipUrl).spec()},
        {accuracy_tips::features::kNumIgnorePrompts.name, "1"}};
    feature_list_.InitWithFeaturesAndParameters(
        {{safe_browsing::kAccuracyTipsFeature, accuraty_tips_params}}, {});
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewHttpBrowserTest,
                       ShowOnUrlInListButNotSecure) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetHttpUrl(kAccuracyTipUrl)));
  EXPECT_FALSE(IsUIShowing());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  EXPECT_TRUE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_EQ(location_bar_view->location_icon_view()->GetText(),
            l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE));
}

// Render test for accuracy tip ui.
class AccuracyTipBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    bool show_opt_out = name != "ignore_button";

    ShowAccuracyTipDialog(browser()->tab_strip_model()->GetActiveWebContents(),
                          accuracy_tips::AccuracyTipStatus::kShowAccuracyTip,
                          show_opt_out, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewDialogBrowserTest,
                       InvokeUi_ignore_button) {
  ShowAndVerifyUi();
}

class AccuracyTipBubbleViewPrerenderBrowserTest
    : public AccuracyTipBubbleViewBrowserTest {
 public:
  AccuracyTipBubbleViewPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &AccuracyTipBubbleViewPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~AccuracyTipBubbleViewPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(https_server());
    AccuracyTipBubbleViewBrowserTest::SetUp();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;

 private:
  void SetUpFeatureList() override {
    const base::FieldTrialParams accuraty_tips_params = {
        {accuracy_tips::features::kSampleUrl.name,
         GetUrl(kAccuracyTipUrl).spec()}};
    feature_list_.InitWithFeaturesAndParameters(
        {{safe_browsing::kAccuracyTipsFeature, accuraty_tips_params}}, {});
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccuracyTipBubbleViewPrerenderBrowserTest,
                       StillShowAfterPrerenderNavigation) {
  // Generate a Accuracy Tip.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kAccuracyTipUrl)));
  EXPECT_TRUE(IsUIShowing());
  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.PageStatus", AccuracyTipStatus::kShowAccuracyTip, 1);

  // Start a prerender.
  prerender_helper_.AddPrerender(
      https_server()->GetURL(kAccuracyTipUrl, "/title2.html"));

  // Ensure the tip isn't closed by prerender navigation and isn't from the
  // prerendered page.
  EXPECT_TRUE(IsUIShowing());
  histogram_tester()->ExpectUniqueSample(
      "Privacy.AccuracyTip.PageStatus", AccuracyTipStatus::kShowAccuracyTip, 1);
}
