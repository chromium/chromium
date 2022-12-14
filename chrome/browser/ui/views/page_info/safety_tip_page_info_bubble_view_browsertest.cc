// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/lookalikes/lookalike_test_helper.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/reputation_web_contents_observer.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/page_info/core/features.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "components/reputation/core/safety_tips.pb.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_state/core/security_state.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/test/test_event.h"
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

// A single test case for UKM collection on triggered heuristics.
// |navigated_url| is the URL that will be navigated to, and |expected_results|
// contains the heuristics that are expected to trigger and be recorded via UKM
// data.
struct HeuristicsTestCase {
  GURL navigated_url;
  TriggeredHeuristics expected_results;
};

// Returns the full name for the give interaction histogram.
std::string GetInteractionHistogram(const char* name) {
  return std::string("Security.SafetyTips.Interaction.") + name;
}

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
  ui::test::TestEvent event;
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

// Add an allowlist with entries that aren't allowlisted for all domains.
void ConfigureAllowlistWithScopes() {
  auto config_proto = reputation::GetOrCreateSafetyTipsConfig();
  config_proto->clear_allowed_pattern();
  config_proto->clear_canonical_pattern();
  config_proto->clear_cohort();

  // Note: allowed_pattern must be sorted, so "googlee" comes before "gooogle".

  // googlee.com may spoof google.com
  config_proto->add_canonical_pattern()->set_pattern("google.com/");
  auto* pattern = config_proto->add_allowed_pattern();
  pattern->set_pattern("googlee.com/");
  auto* cohort = config_proto->add_cohort();
  cohort->add_canonical_index(0);  // google.com
  pattern->add_cohort_index(0);

  // gooogle.com may spoof blogspot, but not google.
  config_proto->add_canonical_pattern()->set_pattern("blogspot.com/");
  pattern = config_proto->add_allowed_pattern();
  pattern->set_pattern("gooogle.com/");
  cohort = config_proto->add_cohort();
  cohort->add_canonical_index(1);  // blogspot.com
  pattern->add_cohort_index(1);

  reputation::SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

}  // namespace

class SafetyTipPageInfoBubbleViewBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    reputation::InitializeSafetyTipConfig();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    SetUpLookalikeTestParams();
    // Check that the test top domain list contains google.
    ASSERT_TRUE(IsTopDomain(GetDomainInfo("google.com")));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    TearDownLookalikeTestParams();
    ReputationService::Get(browser()->profile())
        ->ResetWarningDismissedETLDPlusOnesForTesting();
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
    EXPECT_FALSE(bubble->info_link_);
    EXPECT_FALSE(bubble->leave_button_);
  }

  void CloseWarningLeaveSite(Browser* browser) {
    // Wait for a Safety Tip bubble to be destroyed. Navigating away from a page
    // with a safety tip destroys the safety tip, but waiting for the navigation
    // to complete is racy.
    views::test::WidgetDestroyedWaiter waiter(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()->GetWidget());
    ClickLeaveButton();
    waiter.Wait();
  }

  std::u16string GetSafetyTipSummaryText() {
    auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
    auto* summary_label = page_info->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
    return static_cast<views::StyledLabel*>(summary_label)->GetText();
  }

  void CheckPageInfoShowsSafetyTipInfo(
      Browser* browser,
      security_state::SafetyTipStatus expected_safety_tip_status,
      const GURL& expected_safe_url) {
    OpenPageInfoBubble(browser);
    ASSERT_EQ(PageInfoBubbleViewBase::GetShownBubbleType(),
              PageInfoBubbleViewBase::BubbleType::BUBBLE_PAGE_INFO);
    auto* page_info = PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
    ASSERT_TRUE(page_info);

    switch (expected_safety_tip_status) {
      case security_state::SafetyTipStatus::kBadReputation:
      case security_state::SafetyTipStatus::kBadReputationIgnored:
        EXPECT_EQ(GetSafetyTipSummaryText(),
                  l10n_util::GetStringUTF16(
                      IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE));
        break;

      case security_state::SafetyTipStatus::kLookalike:
      case security_state::SafetyTipStatus::kLookalikeIgnored:
        EXPECT_EQ(GetSafetyTipSummaryText(),
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
    content::WebContentsAddedObserver new_tab_observer;
    static_cast<views::StyledLabel*>(
        page_info->GetViewByID(
            PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL))
        ->ClickFirstLinkForTesting();
    EXPECT_EQ(chrome::kSafetyTipHelpCenterURL,
              new_tab_observer.GetWebContents()->GetVisibleURL());
  }

  void CheckPageInfoDoesNotShowSafetyTipInfo(Browser* browser) {
    OpenPageInfoBubble(browser);
    auto* page_info = static_cast<PageInfoBubbleViewBase*>(
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
    ASSERT_TRUE(page_info);
    EXPECT_TRUE(
        GetSafetyTipSummaryText() ==
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY) ||
        GetSafetyTipSummaryText() ==
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE));
    if (PageInfoBubbleViewBase::GetShownBubbleType() ==
        PageInfoBubbleViewBase::BubbleType::BUBBLE_PAGE_INFO) {
      content::WebContentsAddedObserver new_tab_observer;
      static_cast<views::StyledLabel*>(
          page_info->GetViewByID(
              PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL))
          ->ClickFirstLinkForTesting();
      EXPECT_EQ(chrome::kPageInfoHelpCenterURL,
                new_tab_observer.GetWebContents()->GetVisibleURL());
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

// Ensure normal sites with low engagement are not blocked.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnLowEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure normal sites with low engagement are not blocked in incognito.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnLowEngagementIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(incognito_browser, kNavigatedUrl, kLowEngagement);
  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(
      CheckPageInfoDoesNotShowSafetyTipInfo(incognito_browser));
}

// Ensure blocked sites with high engagement are not blocked.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagement) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  SetEngagementScore(browser(), kNavigatedUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites with high engagement are not blocked in incognito.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagementIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
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
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest, ShowOnBlock) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Ensure blocked sites that don't load don't get blocked.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest, NoShowOnError) {
  auto kNavigatedUrl =
      embedded_test_server()->GetURL("site1.com", "/close-socket");

  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites get blocked in incognito.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       ShowOnBlockIncognito) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      incognito_browser, security_state::SafetyTipStatus::kBadReputation,
      GURL()));
}

// Ensure same-document navigations don't close the Safety Tip.
// Regression test for crbug.com/1137661
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       StillShowAfterSameDocNav) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  // Generate a Safety Tip.
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  // Now generate a same-document navigation and verify the tip is still there.
  NavigateToURL(browser(), GURL(kNavigatedUrl.spec() + "#fragment"),
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Ensure explicitly-allowed sites don't get blocked when the site is otherwise
// blocked server-side.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnAllowlist) {
  auto kNavigatedUrl = GetURL("site1.com");

  // Ensure a Safety Tip is triggered initially...
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));

  // ...but suppressed by the allowlist.
  reputation::SetSafetyTipAllowlistPatterns({"site1.com/"}, {}, {});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));

  // TODO(crbug.com/1401102): Only one UKM should have been recorded, but
  // allowlisted domain also records one.
  CheckRecordedHeuristicsUkmCount(2);
}

// Ensure sites allowed by enterprise policy don't get blocked.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
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

  // TODO(crbug.com/1401102): This shouldn't record a UKM.
  CheckRecordedHeuristicsUkmCount(2);
}

// After the user clicks 'leave site', the user should end up on a safe domain.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteLeavesSite) {
  ASSERT_TRUE(IsTopDomain(GetDomainInfo("google.sk")));
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(IsUIShowing());

  CloseWarningLeaveSite(browser());
  EXPECT_FALSE(IsUIShowing());
  EXPECT_NE(kNavigatedUrl, browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetLastCommittedURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Test that clicking 'learn more' opens a help center article.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       LearnMoreOpensHelpCenter) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  content::WebContentsAddedObserver new_tab_observer;
  ClickLearnMoreLink();
  EXPECT_NE(kNavigatedUrl,
            new_tab_observer.GetWebContents()->GetLastCommittedURL());
}

// Test that the Suspicious Site Safety Tip has no buttons and has the correct
// strings.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       SuspiciousSiteUI) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(CheckNoButtons());
  auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_EQ(
      page_info->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE));
}

// If the user clicks 'leave site', the warning should re-appear when the user
// re-visits the page.
// Flaky on Mac: https://crbug.com/1139955
// Flaky in general, test depends on subtle timing, https://crbug.com/1142769
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_LeaveSiteStillWarnsAfter) {
  auto kNavigatedUrl = GetURL("site1.com");

  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl, browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetLastCommittedURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// After the user closes the warning, they should still be on the same domain.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStaysOnPage) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl, browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetLastCommittedURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// If the user closes the bubble, the warning should not re-appear when the user
// re-visits the page, but will still show up in PageInfo.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreWarningStopsWarning) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl, browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetLastCommittedURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputationIgnored,
      GURL()));
}

// Non main-frame navigations should be ignored.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreIFrameNavigations) {
  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
  const GURL kFrameUrl =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  reputation::SetSafetyTipBadRepPatterns({"a.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Background tabs shouldn't open a bubble initially, but should when they
// become visible.
// Fails on Mac for one parameter. https://crbug.com/1285242
#if BUILDFLAG(IS_MAC)
#define MAYBE_BubbleWaitsForVisible DISABLED_BubbleWaitsForVisible
#else
#define MAYBE_BubbleWaitsForVisible BubbleWaitsForVisible
#endif
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       MAYBE_BubbleWaitsForVisible) {
  auto kFlaggedUrl = GetURL("site1.com");

  TriggerWarningFromBlocklist(browser(), kFlaggedUrl,
                              WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_FALSE(IsUIShowing());

  SwitchToTabAndWait(browser(),
                     browser()->tab_strip_model()->active_index() + 1);
  EXPECT_TRUE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Background tabs that are errors shouldn't open a tip initially, and shouldn't
// open when they become visible, either.  Test for crbug.com/1019228.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
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
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips do NOT trigger on lookalike domains that trigger an
// interstitial.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       SkipLookalikeInterstitialed) {
  const GURL kNavigatedUrl = GetURL("googlé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips trigger on lookalike domains that don't qualify for an
// interstitial and Page Info shows Safety Tip information.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnLookalike) {
  ASSERT_TRUE(IsTopDomain(GetDomainInfo("google.sk")));
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kLookalike,
      GURL("https://google.sk")));
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips don't trigger on lookalike domains that are explicitly
// allowed by the allowlist.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoTriggersOnLookalikeAllowlist) {
  ASSERT_TRUE(IsTopDomain(GetDomainInfo("google.sk")));
  // This domain is a lookalike of a top domain not in the top 500.
  const GURL kNavigatedUrl = GetURL("googlé.sk");

  // Ensure a Safety Tip is triggered initially...
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());

  // ...but suppressed by the allowlist.
  views::test::WidgetDestroyedWaiter waiter(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting()->GetWidget());
  reputation::SetSafetyTipAllowlistPatterns({"xn--googl-fsa.sk/"}, {}, {});
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  waiter.Wait();

  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));

  // TODO(crbug.com/1401102): Only one UKM should have been recorded, but
  // allowlisted domain also records one.
  CheckRecordedHeuristicsUkmCount(2);
}

// Tests that Safety Tips don't trigger on lookalike domains that are explicitly
// allowed by the allowlist.
// Note: UKM is tied to the heuristic triggering, so we record no UKM here since
// the heuristic doesn't trigger. This is different from the other allowlist
// where the heuristic triggers, UKM is still recorded, but no UI is shown.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoTriggersOnEmbeddedAllowlist) {
  // This domain is one edit distance from one of a top 500 domain.
  const GURL kNavigatedUrl = GetURL("gooogle.com");

  reputation::SetSafetyTipAllowlistPatterns({}, {"google\\.com"}, {});
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips trigger on lookalike domains with edit distance.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnEditDistance) {
  // This domain is one edit from google.com.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  const GURL kTargetUrl = GetURL("google.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  CloseWarningLeaveSite(browser());
  CheckRecordedHeuristicsUkmCount(1);
}

// Tests that Safety Tips don't trigger when using a scoped allowlist.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       DoesntTriggerOnScopedAllowlist) {
  // This domain is one edit from google.com, but is allowed to spoof google.
  const GURL kNavigatedUrl = GetURL("googlee.com");
  const GURL kTargetUrl = GetURL("google.com");
  ConfigureAllowlistWithScopes();
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));

  // TODO(crbug.com/1401102): This shouldn't record metrics.
  CheckRecordedHeuristicsUkmCount(1);
}

// Tests that Safety Tips trigger when the URL is on the allowlist, but is
// scoped to a different domain.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnWrongScopedAllowlist) {
  // This domain is one edit from google.com, and may spoof blogspot.com...
  const GURL kNavigatedUrl = GetURL("gooogle.com");
  const GURL kTargetUrl = GetURL("google.com");
  ConfigureAllowlistWithScopes();
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  CloseWarningLeaveSite(browser());
  CheckRecordedHeuristicsUkmCount(1);
}

// Tests that Character Swap is enabled for lookalikes matching engaged sites.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnCharacterSwap_SiteEngagement) {
  const GURL kNavigatedUrl = GetURL("character-wsap.com");
  const GURL kTargetUrl = GetURL("character-swap.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  CloseWarningLeaveSite(browser());
  CheckRecordedHeuristicsUkmCount(1);
}

// Same as TriggersOnCharacterSwap_SiteEngagement, but this time
// the match is on the actual hostnames and not skeletons. Note that
// the skeletons of example.com and éxaplme.com don't have exactly
// one character swap (exarnple.com and exaprnle.com)
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnCharacterSwap_SiteEngagement_HostnameMatch) {
  const GURL kNavigatedUrl = GetURL("éxapmle.com");
  const GURL kTargetUrl = GetURL("example.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  CloseWarningLeaveSite(browser());
  CheckRecordedHeuristicsUkmCount(1);
}

// Tests that Character Swap is enabled for lookalikes matching top sites.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnCharacterSwap_TopSite) {
  base::HistogramTester histograms;

  const GURL kNavigatedUrl = GetURL("goolge.com");
  const GURL kTargetUrl = GetURL("google.com");
  // Both the lookalike and the target have low engagement.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  CloseWarningLeaveSite(browser());
  CheckRecordedHeuristicsUkmCount(1);
}

// Tests that a hostname on a safe TLD can spoof another hostname without a
// lookalike warning.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnCharacterSwapSafeTLD_CanSpoof) {
  base::HistogramTester histograms;

  const GURL kNavigatedUrl = GetURL("digitla.gov");
  const GURL kTargetUrl = GetURL("digital.gov");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetEngagementScore(browser(), kTargetUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  // TODO(crbug.com/1401102): This shouldn't record metrics.
  CheckRecordedHeuristicsUkmCount(1);
}

// Navigate to a domain within a character swap of 1 to a top domain,
// but the character swap is at the registry. This should not be flagged
// as a character swap match.
IN_PROC_BROWSER_TEST_F(
    SafetyTipPageInfoBubbleViewBrowserTest,
    DoesntTriggerOnCharacterSwap_TopSiteWithDifferentRegistry) {
  ASSERT_TRUE(IsTopDomain(GetDomainInfo("google.rs")));

  base::HistogramTester histograms;
  // google.sr is within one character swap of google.rs which is a top domain.
  const GURL kNavigatedUrl = GetURL("google.sr");
  // Even if the navigated site has a low engagement score, it should be
  // considered for lookalike suggestions.
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 0);
  CheckRecordedHeuristicsUkmCount(0);
}

// Tests that Safety Tips trigger on lookalike domains with tail embedding when
// enabled, and not otherwise.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggersOnTailEmbedding) {
  // Tail-embedding has the canonical domain at the very end of the lookalike.
  const GURL kNavigatedUrl = GetURL("accounts-google.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
}

// Tests that Safety Tips don't trigger on lookalike domains with non-tail
// target embedding.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       DoesntTriggersOnGenericTargetEmbedding) {
  const GURL kNavigatedUrl = GetURL("google.com.evil.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
}

// Tests that the SafetyTipShown histogram triggers correctly.
// Flaky on all platforms: https://crbug.com/1139955
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
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
  CloseWarningLeaveSite(browser());
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputation, 1);

  const GURL kLookalikeUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kLookalikeUrl, kLowEngagement);
  NavigateToURL(browser(), kLookalikeUrl, WindowOpenDisposition::CURRENT_TAB);

  // Verify metrics for lookalike domains.
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kLookalike, 1);
  histograms.ExpectTotalCount(kHistogramName, 3);
}

// Tests that the SafetyTipIgnoredPageLoad histogram triggers correctly.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       SafetyTipIgnoredPageLoadHistogram) {
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

// Tests that Safety Tip interactions are recorded in a histogram when the user
// leaves the site using the safety tip.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram_LeaveSite) {
  base::HistogramTester histogram_tester;
  const GURL kLookalikeUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kLookalikeUrl, kLowEngagement);
  NavigateToURL(browser(), kLookalikeUrl, WindowOpenDisposition::CURRENT_TAB);
  // The histogram should not be recorded until the user has interacted with
  // the safety tip.
  histogram_tester.ExpectTotalCount(
      GetInteractionHistogram("SafetyTip_Lookalike"), 0);

  CloseWarningLeaveSite(browser());
  histogram_tester.ExpectUniqueSample(
      GetInteractionHistogram("SafetyTip_Lookalike"),
      SafetyTipInteraction::kLeaveSite, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram when the user
// dismisses the Safety Tip.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram_DismissWithClose) {
  base::HistogramTester histogram_tester;
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  // The histogram should not be recorded until the user has interacted with
  // the safety tip.
  histogram_tester.ExpectTotalCount(
      GetInteractionHistogram("SafetyTip_BadReputation"), 0);
  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kDismiss, 1);
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kDismissWithClose, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram when the user
// dismisses the Safety Tip using ESC key.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram_DismissWithEsc) {
  // Test that the specific dismissal type is recorded correctly.
  base::HistogramTester histogram_tester;
  auto kNavigatedUrl = GetURL("site2.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  CloseWarningIgnore(views::Widget::ClosedReason::kEscKeyPressed);
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kDismiss, 1);
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kDismissWithEsc, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram.
// Flaky in general: Closing the tab may or may not run the callbacks.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       DISABLED_InteractionsHistogram_CloseTab) {
  // Test that tab close is recorded properly.
  base::HistogramTester histogram_tester;
  auto kNavigatedUrl = GetURL("site3.com");

  // Prep the web contents for later observing.
  NavigateToURL(browser(), GURL("about:blank"),
                WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ReputationWebContentsObserver* rep_observer =
      ReputationWebContentsObserver::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Trigger the warning in the prepped web contents.
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);

  // Close the current tab and wait for that to happen.
  base::RunLoop loop;
  rep_observer->RegisterSafetyTipCloseCallbackForTesting(loop.QuitClosure());
  chrome::CloseTab(browser());
  loop.Run();

  // Verify histograms.
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kCloseTab, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram when the user
// switches tabs.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram_SwitchTab) {
  // Test that tab switch is recorded properly.
  ReputationWebContentsObserver* rep_observer =
      ReputationWebContentsObserver::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  base::RunLoop loop;
  rep_observer->RegisterSafetyTipCloseCallbackForTesting(loop.QuitClosure());

  base::HistogramTester histogram_tester;
  auto kNavigatedUrl = GetURL("site4.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  NavigateToURL(browser(), GURL("about:blank"),
                WindowOpenDisposition::NEW_FOREGROUND_TAB);
  loop.Run();
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kSwitchTab, 1);
}

// Tests that Safety Tip interactions are recorded in a histogram when the user
// navigates away from the site.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       InteractionsHistogram_NavigateAway) {
  // Test that navigating away is recorded properly.
  ReputationWebContentsObserver* rep_observer =
      ReputationWebContentsObserver::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  base::RunLoop loop;
  rep_observer->RegisterSafetyTipCloseCallbackForTesting(loop.QuitClosure());

  base::HistogramTester histogram_tester;
  auto kNavigatedUrl = GetURL("site5.com");
  TriggerWarningFromBlocklist(browser(), kNavigatedUrl,
                              WindowOpenDisposition::CURRENT_TAB);
  NavigateToURL(browser(), GURL("about:blank"),
                WindowOpenDisposition::CURRENT_TAB);
  loop.Run();
  histogram_tester.ExpectBucketCount(
      GetInteractionHistogram("SafetyTip_BadReputation"),
      SafetyTipInteraction::kChangePrimaryPage, 1);
}

// Tests that Safety Tips aren't triggered on 'unknown' flag types from the
// component updater. This permits us to add new flag types to the component
// without breaking this release.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
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
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotShownOnYoungDomain) {
  auto kNavigatedUrl = GetURL("site1.com");
  reputation::SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, reputation::FlaggedPage::YOUNG_DOMAIN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure that a metrics-only heuristic doesn't show up in PageInfo. Also
// a regression test for crbug/1061244.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       MetricsOnlyHeuristicDoesntShowInPageInfo) {
  // This URL will trigger Combo Squatting. Combo Squatting UI is disabled by
  // default so this should only record metrics.
  const GURL kNavigatedUrl = GetURL("google-login.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  base::HistogramTester histograms;
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  // Make sure that the UI isn't showing but interstitial histogram is recorded.
  ASSERT_FALSE(IsUIShowing());
  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kComboSquatting, 1);

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Tests that UKM data gets properly recorded when safety tip heuristics get
// triggered.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       HeuristicsUkmRecorded) {
  SetEngagementScore(browser(), GURL("https://google.com"), kHighEngagement);
  SetEngagementScore(browser(), GURL("https://youtube.com"), kHighEngagement);

  // Note that we only want the lookalike heuristic to trigger when our UI
  // status is fully enabled (if it's not, our lookalike heuristic shouldn't
  // trigger).
  const std::vector<HeuristicsTestCase> test_cases = {
      /*blocklist*/ /*lookalike*/ /*keywords*/
      {GetURL("test.com"), {false, false, false}},
      {GetURL("googlee.com"), {false, true, false}},
      // Following tests are disabled because the blocklisted UI doesn't have a
      // "Leave" button.
      // TODO(crbug.com/1386300): Remove once the blocklist heuristic is
      // deleted.
      // {GetURL("youtubee.com"), {true, true, false}},
      // {GetURL("blocklist.com"), {true, false, false}},
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
         test_case.expected_results.blocklist_heuristic_triggered)) {
      CloseWarningLeaveSite(browser());
    }
  }

  size_t expected_event_count = base::ranges::count_if(
      test_cases, [](const HeuristicsTestCase& test_case) {
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
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       WarningDismissalCausesUkmRecordingForLookalike) {
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
IN_PROC_BROWSER_TEST_F(
    SafetyTipPageInfoBubbleViewBrowserTest,
    DISABLED_WarningDismissalCausesUkmRecordingForBlocklist) {
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

// Test that a Safety Tip is shown and metrics are recorded when
// a combo squatting url is flagged with a hard-coded brand name.
// This test case trigger `keyword` heuristic as well because of `google`
// in the URL.
// TODO(crbug.com/1343630): keyword (embedded keyword) heuristic should
// be removed from the code including CheckHeuristicsUkmRecord.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggerOnComboSquatting) {
  // Set a launch config with 100% rollout for Combo Squatting.
  reputation::AddSafetyTipHeuristicLaunchConfigForTesting(
      reputation::HeuristicLaunchConfig::HEURISTIC_COMBO_SQUATTING_TOP_DOMAINS,
      100);
  base::HistogramTester histograms;
  const GURL kNavigatedUrl = GetURL("google-login.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kComboSquatting, 1);

  // Make sure that the UI is now showing, and that no UKM data has been
  // recorded yet.
  ASSERT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  // Once we close the warning, ensure that the UI is no longer showing, and
  // that UKM data has now been recorded.
  CloseWarningLeaveSite(browser());
  ASSERT_FALSE(IsUIShowing());

  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord(
      {kNavigatedUrl,
       {/*blocklist=*/false, /*lookalike=*/true, /*keywords=*/false}},
      0);

  // Navigate to the same site again, but close the warning with an ignore
  // instead of an accept. This should still record UKM data.
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  ASSERT_TRUE(IsUIShowing());

  // Make sure the already collected UKM data still exists.
  CheckRecordedHeuristicsUkmCount(1);
  CheckHeuristicsUkmRecord(
      {kNavigatedUrl,
       {/*blocklist=*/false, /*lookalike=*/true, /*keywords=*/false}},
      0);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(2);
  CheckHeuristicsUkmRecord(
      {kNavigatedUrl,
       {/*blocklist=*/false, /*lookalike=*/true, /*keywords=*/false}},
      0);
  CheckHeuristicsUkmRecord(
      {kNavigatedUrl,
       {/*blocklist=*/false, /*lookalike=*/true, /*keywords=*/false}},
      1);
}

// Test that a Safety Tip is shown and metrics are recorded when
// a combo squatting url is flagged with a hard-coded brand name.
// In contrast with `TriggerOnComboSquatting`, this test case only
// triggers `lookalike` heuristic.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggerOnlyOnComboSquatting) {
  // Set a launch config with 100% rollout for Combo Squatting.
  reputation::AddSafetyTipHeuristicLaunchConfigForTesting(
      reputation::HeuristicLaunchConfig::HEURISTIC_COMBO_SQUATTING_TOP_DOMAINS,
      100);
  base::HistogramTester histograms;
  const GURL kNavigatedUrl = GetURL("costco-login.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kComboSquatting, 1);

  // Make sure that the UI is now showing, and that no UKM data has been
  // recorded yet.
  ASSERT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  // Once we close the warning, ensure that the UI is no longer showing, and
  // that UKM data has now been recorded.
  CloseWarningLeaveSite(browser());
  ASSERT_FALSE(IsUIShowing());

  CheckRecordedHeuristicsUkmCount(1);
  // Boolean values are /*blocklist*/ /*lookalike*/ /*keywords*/
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
  // Boolean values are /*blocklist*/ /*lookalike*/ /*keywords*/
  // The last `false` is different from the previous test because
  // `keywords heuristic` is not triggered by this test case.
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 0);
  CheckHeuristicsUkmRecord({kNavigatedUrl, {false, true, false}}, 1);
  // TODO(crbug.com/1343630): keyword (embedded keyword) heuristic should
  // be removed from the code including CheckHeuristicsUkmRecord.
}

// Test that a Safety Tip is shown and metrics are recorded when
// a combo squatting url is flagged with a brand name from engaged sites.
// In this test case, engaged site is not one of the keywords in `keyword`
// heuristic.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       TriggerOnComboSquattingSiteEngagement) {
  // Set a launch config with 100% rollout for Combo Squatting.
  reputation::AddSafetyTipHeuristicLaunchConfigForTesting(
      reputation::HeuristicLaunchConfig::
          HEURISTIC_COMBO_SQUATTING_ENGAGED_SITES,
      100);
  base::HistogramTester histograms;
  const GURL kEngagedUrl = GetURL("example.com");
  const GURL kNavigatedUrl = GetURL("example-login.com");
  SetEngagementScore(browser(), kEngagedUrl, kHighEngagement);
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(
      lookalikes::kHistogramName,
      NavigationSuggestionEvent::kComboSquattingSiteEngagement, 1);

  // Make sure that the UI is now showing, and that no UKM data has been
  // recorded yet.
  ASSERT_TRUE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);

  // Once we close the warning, ensure that the UI is no longer showing, and
  // that UKM data has now been recorded.
  CloseWarningLeaveSite(browser());
  ASSERT_FALSE(IsUIShowing());

  CheckRecordedHeuristicsUkmCount(1);
  // Boolean values are /*blocklist*/ /*lookalike*/ /*keywords*/
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

// This test checks that Safety Tip is not showing when the Combo Squatting
// is not enabled for hard coded list by gradual roll out.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotTriggerOnComboSquattingButNotLaunched) {
  base::HistogramTester histograms;
  const GURL kNavigatedUrl = GetURL("costco-login.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(lookalikes::kHistogramName,
                               NavigationSuggestionEvent::kComboSquatting, 1);

  // Make sure that the UI is not showing, and that no UKM data has been
  // recorded.
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);
}

// This test checks that Safety Tip is not showing when the Combo Squatting
// is not enabled for engaged sites by gradual roll out.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewBrowserTest,
                       NotTriggerOnComboSquattingSiteEngagementNotLaunched) {
  base::HistogramTester histograms;
  const GURL kEngagedUrl = GetURL("example.com");
  const GURL kNavigatedUrl = GetURL("example-login.com");
  SetEngagementScore(browser(), kEngagedUrl, kHighEngagement);
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  histograms.ExpectTotalCount(lookalikes::kHistogramName, 1);
  histograms.ExpectBucketCount(
      lookalikes::kHistogramName,
      NavigationSuggestionEvent::kComboSquattingSiteEngagement, 1);

  // Make sure that the UI is not showing, and that no UKM data has been
  // recorded.
  ASSERT_FALSE(IsUIShowing());
  CheckRecordedHeuristicsUkmCount(0);
}

class SafetyTipPageInfoBubbleViewPrerenderBrowserTest
    : public InProcessBrowserTest {
 public:
  SafetyTipPageInfoBubbleViewPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SafetyTipPageInfoBubbleViewPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~SafetyTipPageInfoBubbleViewPrerenderBrowserTest() override = default;
  SafetyTipPageInfoBubbleViewPrerenderBrowserTest(
      const SafetyTipPageInfoBubbleViewPrerenderBrowserTest&) = delete;
  SafetyTipPageInfoBubbleViewPrerenderBrowserTest& operator=(
      const SafetyTipPageInfoBubbleViewPrerenderBrowserTest&) = delete;

  void SetUp() override {
    reputation::InitializeSafetyTipConfig();
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that ReputationWebContentsObserver only checks heuristics when the
// primary page navigates. It loads a page in the prerenderer, verifies that
// heuristics were not run, then navigates to the prerendered site, and verifies
// that heuristics are then run.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewPrerenderBrowserTest,
                       SafetyTipOnPrerender) {
  // Start test server.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop_for_prerenderer;
  auto* rep_observer =
      ReputationWebContentsObserver::FromWebContents(web_contents());
  ASSERT_TRUE(rep_observer);
  rep_observer->reset_reputation_check_pending_for_testing();
  rep_observer->RegisterReputationCheckCallbackForTesting(
      run_loop_for_prerenderer.QuitClosure());

  ASSERT_TRUE(rep_observer->reputation_check_pending_for_testing());
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  // Loads |prerender_url| in the prerenderer.
  auto prerender_id = prerender_helper()->AddPrerender(prerender_url);
  ASSERT_NE(content::RenderFrameHost::kNoFrameTreeNodeId, prerender_id);
  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     prerender_id);
  // Waits until ReputationWebContentsObserver calls the callback.
  run_loop_for_prerenderer.Run();
  // |reputation_check_pending_for_testing_| is not updated since
  // ReputationWebContentsObserver ignores the prerenderer.
  ASSERT_TRUE(rep_observer->reputation_check_pending_for_testing());

  base::RunLoop run_loop_for_primary;
  rep_observer->reset_reputation_check_pending_for_testing();
  rep_observer->RegisterReputationCheckCallbackForTesting(
      run_loop_for_primary.QuitClosure());
  ASSERT_TRUE(rep_observer->reputation_check_pending_for_testing());
  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Waits until ReputationWebContentsObserver calls the callback.
  run_loop_for_primary.Run();

  // |reputation_check_pending_for_testing_| is updated to false as
  // ReputationWebContentsObserver works with the primary page.
  ASSERT_FALSE(rep_observer->reputation_check_pending_for_testing());

  // Make sure that the prerender was activated when the main frame was
  // navigated to the prerender_url.
  ASSERT_TRUE(host_observer.was_activated());
}

// Ensure prerender navigations don't close the Safety Tip.
IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewPrerenderBrowserTest,
                       StillShowAfterPrerenderNavigation) {
  GURL url = embedded_test_server()->GetURL("site1.com", "/title1.html");
  reputation::SetSafetyTipBadRepPatterns({"site1.com/"});

  base::HistogramTester histograms;
  const char kHistogramName[] = "Security.SafetyTips.SafetyTipShown";

  // Generate a Safety Tip.
  content::TestNavigationObserver navigation_observer(web_contents());
  NavigateToURL(browser(), url, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowing());
  histograms.ExpectTotalCount(kHistogramName, 1);

  // Wait until the primary page is loaded and start a prerender.
  navigation_observer.Wait();
  prerender_helper()->AddPrerender(
      embedded_test_server()->GetURL("site1.com", "/title2.html"));

  // Ensure the tip isn't closed by prerender navigation and isn't from the
  // prerendered page.
  EXPECT_TRUE(IsUIShowing());
  histograms.ExpectTotalCount(kHistogramName, 1);
}

class SafetyTipPageInfoBubbleViewDialogTest : public DialogBrowserTest {
 public:
  SafetyTipPageInfoBubbleViewDialogTest() = default;
  SafetyTipPageInfoBubbleViewDialogTest(
      const SafetyTipPageInfoBubbleViewDialogTest&) = delete;
  SafetyTipPageInfoBubbleViewDialogTest& operator=(
      const SafetyTipPageInfoBubbleViewDialogTest&) = delete;
  ~SafetyTipPageInfoBubbleViewDialogTest() override = default;

  void ShowUi(const std::string& name) override {
    auto status = security_state::SafetyTipStatus::kUnknown;
    if (name == "BadReputation")
      status = security_state::SafetyTipStatus::kBadReputation;
    else if (name == "Lookalike")
      status = security_state::SafetyTipStatus::kLookalike;

    ShowSafetyTipDialog(browser()->tab_strip_model()->GetActiveWebContents(),
                        status, GURL("https://www.google.tld"),
                        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewDialogTest,
                       InvokeUi_BadReputation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SafetyTipPageInfoBubbleViewDialogTest,
                       InvokeUi_Lookalike) {
  ShowAndVerifyUi();
}
