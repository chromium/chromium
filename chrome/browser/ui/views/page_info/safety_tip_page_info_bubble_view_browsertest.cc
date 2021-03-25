// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/reputation_web_contents_observer.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "components/reputation/core/safety_tips.pb.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/range/range.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

enum class UIStatus {
  kDisabled,
  kEnabledWithDefaultFeatures,
  kEnabledWithSuspiciousSites,
  kEnabledWithAllFeatures,
};

// An engagement score above MEDIUM.
const int kHighEngagement = 20;

// An engagement score below MEDIUM.
const int kLowEngagement = 1;

class ClickEvent : public ui::Event {
 public:
  ClickEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

// A single test case for UKM collection on triggered heuristics.
// |navigated_url| is the URL that will be navigated to, and |expected_results|
// contains the heuristics that are expected to trigger and be recorded via UKM
// data.
struct HeuristicsTestCase {
  GURL navigated_url;
  TriggeredHeuristics expected_results;
};

// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
//
// This function waits for the reputation check to complete.
void NavigateToURL(Browser* browser,
                   const GURL& url,
                   WindowOpenDisposition disposition) {
  // If we plan to use an existing tab, ensure that it's latch is reset.
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    // Null web contents happen when you first create an incognito browser,
    // since it doesn't create the tab until first navigation.
    if (contents) {
      ReputationWebContentsObserver* rep_observer =
          ReputationWebContentsObserver::FromWebContents(contents);
      rep_observer->reset_reputation_check_pending_for_testing();
    }
  }
  // Otherwise*, we're creating a new tab and we don't need to do anything.
  // (* Unless we're using SWITCH_TO_TAB. Then the above code will fetch the
  //    wrong tab, so we forbid it since we don't presently need it.)
  CHECK_NE(disposition, WindowOpenDisposition::SWITCH_TO_TAB);

  // Now actually navigate.
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = disposition;
  params.is_renderer_initiated = true;
  Navigate(&params);
  // (Note that we don't need to wait for the load to finish, since we're
  //  waiting for a reputation check, which will happen even later.)

  // If there's still a reputation check pending, wait for it to complete.
  ReputationWebContentsObserver* rep_observer =
      ReputationWebContentsObserver::FromWebContents(
          params.navigated_or_inserted_contents);
  if (rep_observer->reputation_check_pending_for_testing()) {
    base::RunLoop loop;
    rep_observer->RegisterReputationCheckCallbackForTesting(loop.QuitClosure());
    loop.Run();
  }
}

// Stall execution by at least |delay|. Used for ensuring durations measured for
// metrics are approximately correctly. DO NOT use for synchronization.
void DelayAtLeast(const base::TimeDelta delay) {
  // PostDelayedTask guarantees a delay of at least the amount provided, so it's
  // sufficient to just wait for the run loop to make its way through the queue.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delay);
  run_loop.Run();
}

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

bool IsUIShowing() {
  return PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP ==
         PageInfoBubbleViewBase::GetShownBubbleType();
}

void CloseWarningIgnore(views::Widget::ClosedReason reason) {
  if (!PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()) {
    return;
  }
  auto* widget =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()->GetWidget();
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->CloseWithReason(reason);
  waiter.Wait();
}

// Sets the absolute Site Engagement |score| for the testing origin.
void SetEngagementScore(Browser* browser, const GURL& url, double score) {
  site_engagement::SiteEngagementService::Get(browser->profile())
      ->ResetBaseScoreForURL(url, score);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ClickEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Go to |url| in such a way as to trigger a bad reputation safety tip, by
// adding the given URL to the bad reputation blocklist. This is
// just for convenience, since how we trigger warnings will change. Even if the
// warning is triggered, it may not be shown if the URL is opened in the
// background.
//
// This function blocks the entire host + path, ignoring query parameters.
void TriggerWarningFromBlocklist(Browser* browser,
                                 const GURL& url,
                                 WindowOpenDisposition disposition) {
  std::string host;
  std::string path;
  std::string query;
  safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &host, &path,
                                                        &query);
  // For simplicity, ignore query
  reputation::SetSafetyTipBadRepPatterns({host + path});
  SetEngagementScore(browser, url, kLowEngagement);
  NavigateToURL(browser, url, disposition);
}

// Switches the tab at |tab_index| to the foreground, and waits for the
// OnVisibilityChanged reputation check to complete.
void SwitchToTabAndWait(const Browser* browser, int tab_index) {
  base::RunLoop loop;
  auto* tab_strip = browser->tab_strip_model();
  auto* bg_tab = tab_strip->GetWebContentsAt(tab_index);
  EXPECT_NE(browser->tab_strip_model()->active_index(), tab_index);
  ReputationWebContentsObserver* rep_observer =
      ReputationWebContentsObserver::FromWebContents(bg_tab);

  rep_observer->RegisterReputationCheckCallbackForTesting(loop.QuitClosure());
  tab_strip->ActivateTabAt(tab_index);
  if (rep_observer->reputation_check_pending_for_testing()) {
    loop.Run();
  }
}

}  // namespace

class SafetyTipPageInfoBubbleViewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<UIStatus> {
 protected:
  UIStatus ui_status() const { return GetParam(); }
  virtual bool digital_asset_links_enabled() const { return false; }

  bool IsSuspiciousSiteWarningEnabled() const {
    return ui_status() == UIStatus::kEnabledWithDefaultFeatures ||
           ui_status() == UIStatus::kEnabledWithSuspiciousSites ||
           ui_status() == UIStatus::kEnabledWithAllFeatures;
  }

  bool AreLookalikeWarningsEnabled() const {
    // By default, lookalike detection is enabled unless explicitly turned off.
    return ui_status() == UIStatus::kEnabledWithDefaultFeatures ||
           ui_status() == UIStatus::kEnabledWithAllFeatures;
  }

  void SetUp() override {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    std::vector<base::Feature> disabled_features;

    switch (ui_status()) {
      case UIStatus::kDisabled:
        disabled_features.push_back(security_state::features::kSafetyTipUI);
        break;
      case UIStatus::kEnabledWithDefaultFeatures:
        enabled_features.emplace_back(security_state::features::kSafetyTipUI,
                                      base::FieldTrialParams());
        break;
      case UIStatus::kEnabledWithSuspiciousSites:
        enabled_features.emplace_back(
            security_state::features::kSafetyTipUI,
            base::FieldTrialParams({{"suspicioussites", "true"},
                                    {"topsites", "false"},
                                    {"editdistance", "false"},
                                    {"editdistance_siteengagement", "false"},
                                    {"targetembedding", "false"}}));
        break;
      case UIStatus::kEnabledWithAllFeatures:
        enabled_features.emplace_back(
            security_state::features::kSafetyTipUI,
            base::FieldTrialParams({{"suspicioussites", "true"},
                                    {"topsites", "true"},
                                    {"editdistance", "true"},
                                    {"editdistance_siteengagement", "true"},
                                    {"targetembedding", "true"}}));
        enabled_features.emplace_back(
            lookalikes::features::kDetectTargetEmbeddingLookalikes,
            base::FieldTrialParams());
        break;
    }

    if (digital_asset_links_enabled()) {
      enabled_features.emplace_back(
          lookalikes::features::kLookalikeDigitalAssetLinks,
          base::FieldTrialParams());
    } else {
      disabled_features.push_back(
          lookalikes::features::kLookalikeDigitalAssetLinks);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    reputation::InitializeSafetyTipConfig();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  GURL GetURL(const char* hostname) const {
    return embedded_test_server()->GetURL(hostname, "/title1.html");
  }

  GURL GetURLWithoutPath(const char* hostname) const {
    return GetURL(hostname).GetWithEmptyPath();
  }

  void ClickLeaveButton() {
    // This class is a friend to SafetyTipPageInfoBubbleView.
    auto* bubble = static_cast<SafetyTipPageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    PerformMouseClickOnView(bubble->GetLeaveButtonForTesting());
  }

  void ClickLearnMoreLink() {
    // This class is a friend to SafetyTipPageInfoBubbleView.
    auto* bubble = static_cast<SafetyTipPageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    bubble->OpenHelpCenter();
  }

  void CheckNoButtons() {
    auto* bubble = static_cast<SafetyTipPageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    EXPECT_FALSE(bubble->info_button_);
    EXPECT_FALSE(bubble->ignore_button_);
    EXPECT_FALSE(bubble->leave_button_);
  }

  void CloseWarningLeaveSite(Browser* browser) {
    if (ui_status() == UIStatus::kDisabled) {
      return;
    }
    content::TestNavigationObserver navigation_observer(
        browser->tab_strip_model()->GetActiveWebContents(), 1);
    ClickLeaveButton();
    navigation_observer.Wait();
  }

  bool IsUIShowingOrDisabled() {
    return ui_status() == UIStatus::kDisabled ? true : IsUIShowing();
  }

  bool IsUIShowingOrSuspiciousSitesDisabled() {
    return IsSuspiciousSiteWarningEnabled() ? IsUIShowing() : !IsUIShowing();
  }

  bool IsUIShowingOrAllFeaturesEnabled() {
    return AreLookalikeWarningsEnabled() ? IsUIShowing() : !IsUIShowing();
  }

  void CheckPageInfoShowsSafetyTipInfo(
      Browser* browser,
      security_state::SafetyTipStatus expected_safety_tip_status,
      const GURL& expected_safe_url) {
    if (ui_status() == UIStatus::kDisabled) {
      return;
    }

    OpenPageInfoBubble(browser);
    ASSERT_EQ(PageInfoBubbleViewBase::GetShownBubbleType(),
              PageInfoBubbleViewBase::BubbleType::BUBBLE_PAGE_INFO);
    auto* page_info = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    ASSERT_TRUE(page_info);

    switch (expected_safety_tip_status) {
      case security_state::SafetyTipStatus::kBadReputation:
      case security_state::SafetyTipStatus::kBadReputationIgnored:
        EXPECT_EQ(page_info->GetWindowTitle(),
                  l10n_util::GetStringUTF16(
                      IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE));
        break;

      case security_state::SafetyTipStatus::kLookalike:
      case security_state::SafetyTipStatus::kLookalikeIgnored:
        EXPECT_EQ(page_info->GetWindowTitle(),
                  l10n_util::GetStringFUTF16(
                      IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE,
                      security_interstitials::common_string_util::
                          GetFormattedHostName(expected_safe_url)));
        break;

      case security_state::SafetyTipStatus::kDigitalAssetLinkMatch:
      case security_state::SafetyTipStatus::kBadKeyword:
      case security_state::SafetyTipStatus::kUnknown:
      case security_state::SafetyTipStatus::kNone:
        NOTREACHED();
        break;
    }
    EXPECT_EQ(page_info->GetSecurityDescriptionType(),
              PageInfoUI::SecurityDescriptionType::SAFETY_TIP);
  }

  void CheckPageInfoDoesNotShowSafetyTipInfo(Browser* browser) {
    OpenPageInfoBubble(browser);
    auto* page_info = static_cast<PageInfoBubbleViewBase*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    ASSERT_TRUE(page_info);
    EXPECT_TRUE(
        page_info->GetWindowTitle() ==
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY) ||
        page_info->GetWindowTitle() ==
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE));
    if (PageInfoBubbleViewBase::GetShownBubbleType() ==
        PageInfoBubbleViewBase::BubbleType::BUBBLE_PAGE_INFO) {
      EXPECT_NE(static_cast<PageInfoBubbleView*>(page_info)
                    ->GetSecurityDescriptionType(),
                PageInfoUI::SecurityDescriptionType::SAFETY_TIP);
    }
  }

  // Checks that a certain amount of safety tip heuristics UKM events have been
  // recorded.
  void CheckRecordedHeuristicsUkmCount(size_t expected_event_count) {
    std::vector<const ukm::mojom::UkmEntry*> entries =
        test_ukm_recorder_->GetEntriesByName(
            ukm::builders::Security_SafetyTip::kEntryName);
    ASSERT_EQ(expected_event_count, entries.size());
  }

  // Checks that the metrics specified in |test_case| are properly recorded,
  // at the index in the UKM data specified by |expected_idx|.
  void CheckHeuristicsUkmRecord(const HeuristicsTestCase& test_case,
                                size_t expected_idx) {
    std::vector<const ukm::mojom::UkmEntry*> entries =
        test_ukm_recorder_->GetEntriesByName(
            ukm::builders::Security_SafetyTip::kEntryName);

    EXPECT_LT(expected_idx, entries.size());

    test_ukm_recorder_->ExpectEntrySourceHasUrl(entries[expected_idx],
                                                test_case.navigated_url);

    test_ukm_recorder_->ExpectEntryMetric(
        entries[expected_idx], "TriggeredServerSideBlocklist",
        test_case.expected_results.blocklist_heuristic_triggered);
    test_ukm_recorder_->ExpectEntryMetric(
        entries[expected_idx], "TriggeredKeywordsHeuristics",
        test_case.expected_results.keywords_heuristic_triggered);
    test_ukm_recorder_->ExpectEntryMetric(
        entries[expected_idx], "TriggeredLookalikeHeuristics",
        test_case.expected_results.lookalike_heuristic_triggered);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SafetyTipPageInfoBubbleViewBrowserTest,
    ::testing::Values(UIStatus::kDisabled,
                      // Disabled for flakiness. https://crbug.com/1113105.
                      // UIStatus::kEnabledWithDefaultFeatures,
                      UIStatus::kEnabledWithSuspiciousSites));
// Disabled for flakiness. https://crbug.com/1113105.
// UIStatus::kEnabledWithAllFeatures));

// Ensure normal sites with low engagement are not blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnLowEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure normal sites with low engagement are not blocked in incognito.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnLowEngagementIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(), true));
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(incognito_browser, kNavigatedUrl, kLowEngagement);
  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(
      CheckPageInfoDoesNotShowSafetyTipInfo(incognito_browser));
}

// Ensure blocked sites with high engagement are not blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  SetEngagementScore(browser(), kNavigatedUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites with high engagement are not blocked in incognito.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagementIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(), true));
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  SetEngagementScore(incognito_browser, kNavigatedUrl, kHighEngagement);
  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(
      CheckPageInfoDoesNotShowSafetyTipInfo(incognito_browser));
}

// Ensure blocked sites get blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest, ShowOnBlock) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Ensure blocked sites that don't load don't get blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest, NoShowOnError) {
  auto kNavigatedUrl =
      embedded_test_server()->GetURL("site1.com", "/close-socket");

  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites get blocked in incognito.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       ShowOnBlockIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(), true));
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      incognito_browser, security_state::SafetyTipStatus::kBadReputation,
      GURL()));
}

// Ensure same-document navigations don't close the Safety Tip.
// Regression test for crbug.com/1137661
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       StillShowAfterSameDocNav) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  // Generate a Safety Tip.
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());

  // Now generate a same-document navigation and verify the tip is still there.
  NavigateToURL(browser(), GURL(kNavigatedUrl.spec() + "#fragment"),
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Ensure explicitly-allowed sites don't get blocked when the site is otherwise
// blocked server-side.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnAllowlist) {
  auto kNavigatedUrl = GetURL("site1.com");

  // Ensure a Safety Tip is triggered initially...
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));

  // ...but suppressed by the allowlist.
  reputation::SetSafetyTipAllowlistPatterns({"site1.com/"}, {});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure sites allowed by enterprise policy don't get blocked.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnEnterpriseAllowlist) {
  const std::vector<const char*> kUrls = {"site1.com", "bla.site2.com",
                                          "bla.site3.com"};

  reputation::SetSafetyTipBadRepPatterns({"site1.com/", "site2.com/"});
  SetEnterpriseAllowlistForTesting(browser()->profile()->GetPrefs(),
                                   {"site1.com", "bla.site2.com", "site3.com"});

  for (auto* const url : kUrls) {
    NavigateToURL(browser(), GetURL(url), WindowOpenDisposition::CURRENT_TAB);
    EXPECT_FALSE(IsUIShowing());
    ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
  }
}

// After the user clicks 'leave site', the user should end up on a safe domain.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteLeavesSite) {
  // The suspicious site warning doesn't have call-to-action buttons, so this
  // test only applies to lookalike warnings.
  if (!AreLookalikeWarningsEnabled()) {
    return;
  }

  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(IsUIShowing());

  CloseWarningLeaveSite(browser());
  EXPECT_FALSE(IsUIShowing());
  EXPECT_NE(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Test that clicking 'learn more' opens a help center article.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LearnMoreOpensHelpCenter) {
  if (!IsSuspiciousSiteWarningEnabled()) {
    return;
  }

  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  content::WebContentsAddedObserver new_tab_observer;
  ClickLearnMoreLink();
  EXPECT_NE(kNavigatedUrl, new_tab_observer.GetWebContents()->GetURL());
}

// Test that the Suspicious Site Safety Tip has no buttons and has the correct
// strings.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SuspiciousSiteUI) {
  if (!IsSuspiciousSiteWarningEnabled()) {
    return;
  }

  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(CheckNoButtons());
  auto* page_info = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  EXPECT_EQ(
      page_info->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE));
}

// If the user clicks 'leave site', the warning should re-appear when the user
// re-visits the page.
// Flaky on Mac: https://crbug.com/1139955
// Flaky in general, test depends on subtle timing, https://crbug.com/1142769
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_LeaveSiteStillWarnsAfter) {
  auto kNavigatedUrl = GetURL("site1.com");

  if (!IsSuspiciousSiteWarningEnabled()) {
    return;
  }

  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// After the user closes the warning, they should still be on the same domain.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStaysOnPage) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// If the user closes the bubble, the warning should not re-appear when the user
// re-visits the page, but will still show up in PageInfo.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStopsWarning) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }

  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputationIgnored,
      GURL()));
}

// If the UI is disabled, the page should be 'auto-ignored'.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       AlwaysIgnoresWhenDisabled) {
  if (ui_status() != UIStatus::kDisabled) {
    return;
  }

  auto kNavigatedUrl = GetURL("site1.com");
  const char kHistogramName[] = "Security.SafetyTips.SafetyTipShown";
  base::HistogramTester histograms;

  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputation, 1);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputationIgnored,
      1);

  histograms.ExpectTotalCount(kHistogramName, 2);
}

// Non main-frame navigations should be ignored.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreIFrameNavigations) {
  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
  const GURL kFrameUrl =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  reputation::SetSafetyTipBadRepPatterns({"a.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Background tabs shouldn't open a bubble initially, but should when they
// become visible.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       BubbleWaitsForVisible) {
  auto kFlaggedUrl = GetURL("site1.com");

  TriggerWarningFromBlocklist(browser(), kFlaggedUrl,
                              WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_FALSE(IsUIShowing());

  SwitchToTabAndWait(browser(),
                     browser()->tab_strip_model()->active_index() + 1);
  EXPECT_TRUE(IsUIShowingOrSuspiciousSitesDisabled());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Background tabs that are errors shouldn't open a tip initially, and shouldn't
// open when they become visible, either.  Test for crbug.com/1019228.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoBubbleOnErrorEvenAfterVisible) {
  auto kFlaggedUrl =
      embedded_test_server()->GetURL("site1.com", "/close-socket");

  TriggerWarningFromBlocklist(browser(), kFlaggedUrl,
                              WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_FALSE(IsUIShowing());

  SwitchToTabAndWait(browser(),
                     browser()->tab_strip_model()->active_index() + 1);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that Safety Tips do NOT trigger on lookalike domains that trigger an
// interstitial.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SkipLookalikeInterstitialed) {
  const GURL kNavigatedUrl = GetURL("googlé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
}

// Tests that Safety Tips trigger on lookalike domains that don't qualify for an
// interstitial, but do not impact Page Info.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnLookalike) {
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrAllFeaturesEnabled());

  if (AreLookalikeWarningsEnabled()) {
    ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
        browser(), security_state::SafetyTipStatus::kLookalike,
        GURL("https://google.sk")));
  } else {
    ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
  }
}

// Tests that Safety Tips don't trigger on lookalike domains that are explicitly
// allowed by the allowlist.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoTriggersOnLookalikeAllowlist) {
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");

  // Ensure a Safety Tip is triggered initially...
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingOrAllFeaturesEnabled());

  // ...but suppressed by the allowlist.
  reputation::SetSafetyTipAllowlistPatterns({"xn--googl-fsa.sk/"}, {});
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that Safety Tips don't trigger on lookalike domains that are explicitly
// allowed by the allowlist.
// Note: UKM is tied to the heuristic triggering, so we record no UKM here since
// the heuristic doesn't trigger. This is different from the other allowlist
// where the heuristic triggers, UKM is still recorded, but no UI is shown.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoTriggersOnEmbeddedAllowlist) {
  // This domain is one edit distance from one of a top 500 domain.
  const GURL kNavigatedUrl = GetURL("gooogle.com");

  reputation::SetSafetyTipAllowlistPatterns({}, {"google\\.com"});
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips trigger (or not) on lookalike domains with edit
// distance when enabled, and not otherwise.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnEditDistance) {
  // This domain is an edit distance of one from the top 500.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(IsUIShowing(), AreLookalikeWarningsEnabled());
}

// Tests that the SafetyTipShown histogram triggers correctly.
// Flaky on all platforms: https://crbug.com/1139955
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_SafetyTipShownHistogram) {
  const char kHistogramName[] = "Security.SafetyTips.SafetyTipShown";
  base::HistogramTester histograms;

  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kNone, 1);

  auto kBadRepUrl = GetURL("site2.com");
  TriggerWarningFromBlocklist(browser(), kBadRepUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  if (IsSuspiciousSiteWarningEnabled()) {
    CloseWarningLeaveSite(browser());
  }
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputation, 1);

  const GURL kLookalikeUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kLookalikeUrl, kLowEngagement);
  NavigateToURL(browser(), kLookalikeUrl, WindowOpenDisposition::CURRENT_TAB);

  // Record metrics for lookalike domains unless explicitly disabled.
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kLookalike,
      ui_status() == UIStatus::kEnabledWithSuspiciousSites ? 0 : 1);
  histograms.ExpectTotalCount(kHistogramName, 3);
}

// Tests that the SafetyTipIgnoredPageLoad histogram triggers correctly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SafetyTipIgnoredPageLoadHistogram) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  base::HistogramTester histograms;
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(
      "Security.SafetyTips.SafetyTipIgnoredPageLoad",
      security_state::SafetyTipStatus::kBadReputationIgnored, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram) {
  const std::string kHistogramPrefix = "Security.SafetyTips.Interaction.";

  // These histograms are only recorded when the UI feature is enabled, so bail
  // out when disabled.
  if (ui_status() != UIStatus::kEnabledWithAllFeatures) {
    return;
  }

  {
    // This domain is an edit distance of one from the top 500.
    const GURL kNavigatedUrl = GetURL("goooglé.com");
    SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
    base::HistogramTester histogram_tester;
    NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
    // The histogram should not be recorded until the user has interacted with
    // the safety tip.
    histogram_tester.ExpectTotalCount(kHistogramPrefix + "SafetyTip_Lookalike",
                                      0);
    CloseWarningLeaveSite(browser());
    histogram_tester.ExpectUniqueSample(
        kHistogramPrefix + "SafetyTip_Lookalike",
        SafetyTipInteraction::kLeaveSite, 1);
  }

  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    // The histogram should not be recorded until the user has interacted with
    // the safety tip.
    histogram_tester.ExpectTotalCount(
        kHistogramPrefix + "SafetyTip_BadReputation", 0);
    CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismiss, 1);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismissWithClose, 1);
  }

  // Test that the specific dismissal type is recorded correctly.
  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site2.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    CloseWarningIgnore(views::Widget::ClosedReason::kEscKeyPressed);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismiss, 1);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismissWithEsc, 1);
  }

  // Test that tab close is recorded properly.
  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site2.com");

    // Prep the web contents for later observing.
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::NEW_FOREGROUND_TAB);
    ReputationWebContentsObserver* rep_observer =
        ReputationWebContentsObserver::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());

    // Trigger the warning in the prepped web contents.
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);

    // Close all tabs and wait for that to happen.
    base::RunLoop loop;
    rep_observer->RegisterSafetyTipCloseCallbackForTesting(loop.QuitClosure());
    browser()->tab_strip_model()->CloseAllTabs();
    loop.Run();

    // Verify histograms.
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kCloseTab, 1);
  }

  // Test that tab switch is recorded properly.
  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site2.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::NEW_FOREGROUND_TAB);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kSwitchTab, 1);
  }

  // Test that navigating away is recorded properly.
  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site2.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::CURRENT_TAB);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kStartNewNavigation, 1);
  }
}

// Tests that the histograms recording how long the Safety Tip is open are
// recorded properly.
// Flaky on Mac: https://crbug.com/1139955
// Flaky in general, test depends on subtle timing, https://crbug.com/1142769
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_TimeOpenHistogram) {
  if (!IsSuspiciousSiteWarningEnabled()) {
    return;
  }
  const base::TimeDelta kMinWarningTime = base::TimeDelta::FromMilliseconds(10);

  // Test the histogram for no user action taken.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    // Ensure that the tab is open for more than 0 ms, even in the face of bots
    // with bad clocks.
    DelayAtLeast(kMinWarningTime);
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::CURRENT_TAB);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.StartNewNavigation.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  // Test the histogram for when the user adheres to the warning.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    DelayAtLeast(kMinWarningTime);
    CloseWarningLeaveSite(browser());
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.LeaveSite.SafetyTip_BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  // Test the histogram for when the user dismisses the warning.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    DelayAtLeast(kMinWarningTime);
    CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
    auto base_samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.Dismiss.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, base_samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), base_samples.front().min);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.DismissWithClose.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site2.com");
    TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                                WindowOpenDisposition::CURRENT_TAB);
    DelayAtLeast(kMinWarningTime);
    CloseWarningIgnore(views::Widget::ClosedReason::kEscKeyPressed);
    auto base_samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.Dismiss.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, base_samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), base_samples.front().min);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.DismissWithEsc.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }
}

// Tests that Safety Tips aren't triggered on 'unknown' flag types from the
// component updater. This permits us to add new flag types to the component
// without breaking this release.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotShownOnUnknownFlag) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, reputation::FlaggedPage::UNKNOWN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that Safety Tips aren't triggered on domains flagged as 'YOUNG_DOMAIN'
// in the component. This permits us to use this flag in the component without
// breaking this release.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotShownOnYoungDomain) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, reputation::FlaggedPage::YOUNG_DOMAIN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure that the sensitive-keyword heuristic doesn't show up in PageInfo. Also
// a regression test for crbug/1061244.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SensitiveKeywordHeuristicDoesntShowInPageInfo) {
  const std::vector<const char*> kSensitiveKeywords = {"test"};
  auto kNavigatedUrl = GetURL("test-secure.com");

  ReputationService* rep_service = ReputationService::Get(browser()->profile());
  rep_service->SetSensitiveKeywordsForTesting(kSensitiveKeywords.data(),
                                              kSensitiveKeywords.size());

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that UKM data gets properly recorded when safety tip heuristics get
// triggered.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_HeuristicsUkmRecorded) {
  const std::vector<const char*> kSensitiveKeywords = {"test"};

  ReputationService* rep_service = ReputationService::Get(browser()->profile());
  rep_service->SetSensitiveKeywordsForTesting(kSensitiveKeywords.data(),
                                              kSensitiveKeywords.size());

  // Note that we only want the lookalike heuristic to trigger when our UI
  // status is fully enabled (if it's not, our lookalike heuristic shouldn't
  // trigger).
  const std::vector<HeuristicsTestCase> test_cases = {
      /*blocklist*/ /*lookalike*/ /*keywords*/
      {GetURL("test.com"), {false, false, false}},
      {GetURL("test-secure.com"), {false, false, true}},
      {GetURL("test-insecure.com"), {false, false, true}},
      {GetURL("test-blocklist.com"), {true, false, true}},
      {GetURL("other.com"), {false, false, false}},
      {GetURL("other-insecure.com"), {false, false, false}},
      {GetURL("other-insecure.com"), {false, false, false}},
      {GetURL("noblocklist.com"), {false, false, false}},
      {GetURL("blocklist.com"), {true, false, false}},
      {GetURL("a-normal-site.com"), {false, false, false}},
      {GetURL("googlé.sk"),
       {false, ui_status() != UIStatus::kEnabledWithSuspiciousSites, false}},
      {GetURL("test-secure.com"),
       {true, false,
        true}},  // This test case expects multiple heuristics to trigger.
      {GetURL("googlé.sk"),
       {true, ui_status() != UIStatus::kEnabledWithSuspiciousSites, false}},
  };

  for (const HeuristicsTestCase& test_case : test_cases) {
    // If we want the blocklist heuristic to trigger here, actually make it
    // trigger manually.
    if (test_case.expected_results.blocklist_heuristic_triggered) {
      TriggerWarningFromBlocklist(browser(), test_case.navigated_url,
                                  WindowOpenDisposition::CURRENT_TAB);
    } else {
      SetEngagementScore(browser(), test_case.navigated_url, kLowEngagement);
      NavigateToURL(browser(), test_case.navigated_url,
                    WindowOpenDisposition::CURRENT_TAB);
    }

    // If a warning should show, dismiss it to ensure UKM data gets recorded.
    if ((test_case.expected_results.lookalike_heuristic_triggered ||
         test_case.expected_results.blocklist_heuristic_triggered) &&
        IsSuspiciousSiteWarningEnabled()) {
      CloseWarningLeaveSite(browser());
    }
  }

  size_t expected_event_count =
      std::count_if(test_cases.begin(), test_cases.end(),
                    [](const HeuristicsTestCase& test_case) {
                      return test_case.expected_results.triggered_any();
                    });
  CheckRecordedHeuristicsUkmCount(expected_event_count);

  size_t expected_event_idx = 0;
  for (const HeuristicsTestCase& test_case : test_cases) {
    if (!test_case.expected_results.triggered_any()) {
      continue;
    }
    CheckHeuristicsUkmRecord(test_case, expected_event_idx);
    expected_event_idx++;
  }
}

// Tests that UKM data is only recorded after the safety tip warning is
// dismissed or accepted, for the lookalike heuristic.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       WarningDismissalCausesUkmRecordingForLookalike) {
  // Only test when the lookalike UI is actually enabled.
  if (!AreLookalikeWarningsEnabled()) {
    return;
  }

  GURL kNavigatedUrl = GetURL("googlé.sk");

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  // Make sure that the UI is now showing, and that no UKM data has been
  // recorded yet.
  ASSERT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  // Once we close the warning, ensure that the UI is no longer showing, and
  // that UKM data has now been recorded.
  CloseWarningLeaveSite(browser());
  ASSERT_FALSE(IsUIShowing());

  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 0);

  // Navigate to the same site again, but close the warning with an ignore
  // instead of an accept. This should still record UKM data.
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  ASSERT_TRUE(IsUIShowing());

  // Make sure the already collected UKM data still exists.
  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 0);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(2);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 0);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 1);
}

// Tests that UKM data is only recorded after the safety tip warning is
// dismissed or accepted, for the blocklist heuristic.
// Flaky on all platforms: https://crbug.com/1139955
IN_PROC_BROWSER_TEST_P(
    SafetyTipPageInfoBubbleViewBrowserTest,
    DISABLED_WarningDismissalCausesUkmRecordingForBlocklist) {
  // Only test when any UI is actually enabled.
  if (!IsSuspiciousSiteWarningEnabled()) {
    return;
  }

  GURL kNavigatedUrl = GetURL("www.blocklist.com");

  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  // Make sure that the UI is now showing, and that no UKM data has been
  // recorded yet.
  ASSERT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  // Once we close the warning, ensure that the UI is no longer showing, and
  // that UKM data has now been recorded.
  CloseWarningLeaveSite(browser());
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {true, false, false}}, 0);

  // Navigate to the same site again, but close the warning with an ignore
  // instead of an accept. This should still record UKM data.
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  ASSERT_TRUE(IsUIShowing());

  // Make sure the already collected UKM data still exists.
  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {true, false, false}}, 0);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(2);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {true, false, false}}, 0);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {true, false, false}}, 1);
}

// Test fixture that enables the |kSafetyTipUIForSimplifiedDomainDisplay|
// feature and disables other Safety Tips features.
class SafetyTipSimplifiedDomainPageInfoBubbleViewBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {security_state::features::kSafetyTipUIForSimplifiedDomainDisplay},
        {security_state::features::kSafetyTipUI,
         security_state::features::kSafetyTipUIOnDelayedWarning});
    reputation::InitializeSafetyTipConfig();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Safety Tips for simplified domains feature enables lookalike
// Safety Tips.
IN_PROC_BROWSER_TEST_F(SafetyTipSimplifiedDomainPageInfoBubbleViewBrowserTest,
                       SafetyTipForSimplifiedDomain) {
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL url = embedded_test_server()->GetURL("googlé.sk", "/title1.html");
  SetEngagementScore(browser(), url, kLowEngagement);
  NavigateToURL(browser(), url, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
}

// Tests that the Safety Tips for simplified domains feature does not enable the
// bad reputation Safety Tip.
IN_PROC_BROWSER_TEST_F(SafetyTipSimplifiedDomainPageInfoBubbleViewBrowserTest,
                       SafetyTipForSimplifiedDomainNoBadRep) {
  GURL url = embedded_test_server()->GetURL("site1.com", "/title1.html");
  reputation::SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, reputation::FlaggedPage::BAD_REP);
  SetEngagementScore(browser(), url, kLowEngagement);
  NavigateToURL(browser(), url, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
}

// Tests for Digital Asset Links for lookalike checks.
// TODO(meacer): Refactor the DAL code in LookalikeNavigationThrottle tests and
// reuse here.
class SafetyTipPageInfoBubbleViewDigitalAssetLinksBrowserTest
    : public SafetyTipPageInfoBubbleViewBrowserTest {
 protected:
  struct TestSite {
    std::string hostname;
    std::string manifest;
  };

  bool digital_asset_links_enabled() const override { return true; }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void SetUpManifests(const std::vector<TestSite>& sites) {
    url_loader_interceptor_ = std::make_unique<
        content::URLLoaderInterceptor>(base::BindRepeating(
        &SafetyTipPageInfoBubbleViewDigitalAssetLinksBrowserTest::OnIntercept,
        base::Unretained(this), sites));
  }

  static std::string MakeManifestWithTarget(const char* target_domain,
                                            bool invalid = false) {
    const char* const format = R"([{
        "relation": ["%s"],
        "target": {
          "namespace": "web",
          "site": "https://%s"
        }
      }])";
    // Go through MakeURL to convert target_domain to punycode.
    return base::StringPrintf(format,
                              (invalid ? "junkvalue" : "lookalikes/allowlist"),
                              MakeURL(target_domain).host().c_str());
  }

 private:
  bool OnIntercept(const std::vector<TestSite>& sites,
                   content::URLLoaderInterceptor::RequestParams* params) {
    for (const TestSite& site : sites) {
      if (params->url_request.url == MakeManifestURL(site.hostname)) {
        DCHECK(!site.manifest.empty());
        // Serve manifest contents:
        std::string headers =
            "HTTP/1.1 200 OK\nContent-Type: application/json; "
            "charset=utf-8\n";
        content::URLLoaderInterceptor::WriteResponse(headers, site.manifest,
                                                     params->client.get());
        return true;
      }
      // Serve site's contents:
      if (params->url_request.url == MakeURL(site.hostname)) {
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n",
            "<html>Test page</html>", params->client.get());
        return true;
      }
    }
    return false;
  }

  static GURL MakeManifestURL(const std::string& hostname) {
    return GURL("https://" + hostname + "/.well-known/assetlinks.json");
  }

  static GURL MakeURL(const std::string& hostname) {
    return GURL("https://" + hostname);
  }

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SafetyTipPageInfoBubbleViewDigitalAssetLinksBrowserTest,
    ::testing::Values(UIStatus::kEnabledWithAllFeatures));

// Lookalike and target sites' manifests don't match each other. Show the UI.
// TODO(crbug.com/1191216): Check if there is already an existing manifest
// validation happening and ignore new validation requests.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewDigitalAssetLinksBrowserTest,
                       ShowOnDigitalAssetLinkMismatch) {
  if (!AreLookalikeWarningsEnabled()) {
    return;
  }

  const GURL kNavigatedUrl("https://gooogle.com");
  const GURL kTargetUrl("https://google.com");
  const std::vector<TestSite> sites{
      {kNavigatedUrl.host().c_str(), MakeManifestWithTarget("invalid.host")},
      {kTargetUrl.host().c_str(), MakeManifestWithTarget("invalid.host")},
  };
  SetUpManifests(sites);

  base::HistogramTester histograms;

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(IsUIShowing());

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kLookalikeManifestFailed, 1);
}

// Same as ShowOnDigitalAssetLinkMismatch but with valid manifests.
// An edit distance match would normally display a Safety Tip, but the lookalike
// site properly allowlisted the target site so the Safety Tip is suppressed.
// TODO(crbug.com/1191216): Check if there is already an existing manifest
// validation happening and ignore new validation requests.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewDigitalAssetLinksBrowserTest,
                       NoShowOnDigitalAssetLinkMatch) {
  if (!AreLookalikeWarningsEnabled()) {
    return;
  }

  const GURL kNavigatedUrl("https://gooogle.com");
  const GURL kTargetUrl("https://google.com");
  const std::vector<TestSite> sites{
      {kNavigatedUrl.host().c_str(),
       MakeManifestWithTarget(kTargetUrl.host().c_str())},
      {kTargetUrl.host().c_str(),
       MakeManifestWithTarget(kNavigatedUrl.host().c_str())},
  };
  SetUpManifests(sites);

  base::HistogramTester histograms;

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  ASSERT_FALSE(IsUIShowing());

  histograms.ExpectTotalCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName, 2);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kStarted, 1);
  histograms.ExpectBucketCount(
      DigitalAssetLinkCrossValidator::kEventHistogramName,
      DigitalAssetLinkCrossValidator::Event::kValidationSucceeded, 1);
}
