// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/reputation/reputation_web_contents_observer.h"
#include "chrome/browser/reputation/safety_tip_test_utils.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/reputation/safety_tips.pb.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
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
  kEnabled,
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

// Simulates a link click navigation. We don't use
// ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
// typing the URL, causing the site to have a site engagement score of at
// least LOW.
//
// This function waits for the load to complete since it is based on the
// synchronous ui_test_utils::NavigatToURL.
void NavigateToURL(Browser* browser,
                   const GURL& url,
                   WindowOpenDisposition disposition) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin = url::Origin::Create(GURL("about:blank"));
  params.disposition = disposition;
  params.is_renderer_initiated = true;

  ui_test_utils::NavigateToURL(&params);
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
  SiteEngagementService::Get(browser->profile())
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

// Go to |url| in such a way as to trigger a bad reputation safety tip. This is
// just for convenience, since how we trigger warnings will change. Even if the
// warning is triggered, it may not be shown if the URL is opened in the
// background.
//
// This function blocks the entire host + path, ignoring query parameters.
void TriggerWarning(Browser* browser,
                    const GURL& url,
                    WindowOpenDisposition disposition) {
  std::string host;
  std::string path;
  std::string query;
  safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &host, &path,
                                                        &query);
  // For simplicity, ignore query
  SetSafetyTipBadRepPatterns({host + path});
  SetEngagementScore(browser, url, kLowEngagement);
  NavigateToURL(browser, url, disposition);
}

}  // namespace

class SafetyTipPageInfoBubbleViewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<UIStatus> {
 protected:
  UIStatus ui_status() const { return GetParam(); }

  void SetUp() override {
    switch (ui_status()) {
      case UIStatus::kDisabled:
        feature_list_.InitAndDisableFeature(
            security_state::features::kSafetyTipUI);
        break;
      case UIStatus::kEnabled:
        feature_list_.InitWithFeaturesAndParameters(
            {{security_state::features::kSafetyTipUI,
              {{"topsites", "false"},
               {"editdistance", "false"},
               {"editdistance_siteengagement", "false"}}},
             {features::kLookalikeUrlNavigationSuggestionsUI,
              {{"topsites", "true"}}}},
            {});
        break;
      case UIStatus::kEnabledWithAllFeatures:
        feature_list_.InitWithFeaturesAndParameters(
            {{security_state::features::kSafetyTipUI,
              {{"topsites", "true"},
               {"editdistance", "true"},
               {"editdistance_siteengagement", "true"}}},
             {features::kLookalikeUrlNavigationSuggestionsUI,
              {{"topsites", "true"}}}},
            {});
    }

    InitializeSafetyTipConfig();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
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
    bubble->StyledLabelLinkClicked(bubble->GetLearnMoreLinkForTesting(),
                                   gfx::Range(), 0);
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

  bool IsUIShowingIfEnabled() {
    return ui_status() == UIStatus::kDisabled ? true : IsUIShowing();
  }

  bool IsUIShowingOnlyIfFeaturesEnabled() {
    return ui_status() == UIStatus::kEnabledWithAllFeatures ? IsUIShowing()
                                                            : !IsUIShowing();
  }

  void CheckPageInfoShowsSafetyTipInfo(
      Browser* browser,
      security_state::SafetyTipStatus expected_safety_tip_status,
      const GURL& expected_safe_url) {
    if (ui_status() == UIStatus::kDisabled) {
      return;
    }

    OpenPageInfoBubble(browser);
    auto* page_info = static_cast<PageInfoBubbleViewBase*>(
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
    EXPECT_NE(page_info->GetWindowTitle(),
              l10n_util::GetStringUTF16(
                  IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE));
    EXPECT_NE(page_info->GetSecurityDescriptionType(),
              PageInfoUI::SecurityDescriptionType::SAFETY_TIP);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SafetyTipPageInfoBubbleViewBrowserTest,
                         ::testing::Values(UIStatus::kDisabled,
                                           UIStatus::kEnabled,
                                           UIStatus::kEnabledWithAllFeatures));

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
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(), true));
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
  SetSafetyTipBadRepPatterns({"site1.com/"});

  SetEngagementScore(browser(), kNavigatedUrl, kHighEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Ensure blocked sites with high engagement are not blocked in incognito.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnHighEngagementIncognito) {
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(), true));
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipBadRepPatterns({"site1.com/"});

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
  SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Ensure blocked sites get blocked in incognito.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       ShowOnBlockIncognito) {
  Browser* incognito_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(), true));
  auto kNavigatedUrl = GetURL("site1.com");
  SetSafetyTipBadRepPatterns({"site1.com/"});

  NavigateToURL(incognito_browser, kNavigatedUrl,
                WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      incognito_browser, security_state::SafetyTipStatus::kBadReputation,
      GURL()));
}

// Ensure explicitly-allowed sites don't get blocked when the site is otherwise
// blocked server-side.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       NoShowOnAllowlist) {
  auto kNavigatedUrl = GetURL("site1.com");

  // Ensure a Safety Tip is triggered initially...
  SetSafetyTipBadRepPatterns({"site1.com/"});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));

  // ...but suppressed by the allowlist.
  SetSafetyTipAllowlistPatterns({"site1.com/"});
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// After the user clicks 'leave site', the user should end up on a safe domain.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteLeavesSite) {
  auto kNavigatedUrl = GetURL("site1.com");
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }

  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());
  EXPECT_FALSE(IsUIShowing());
  EXPECT_NE(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}

// Test that clicking 'learn more' opens a help center article.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LearnMoreOpensHelpCenter) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }

  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  content::WebContentsAddedObserver new_tab_observer;
  ClickLearnMoreLink();
  EXPECT_NE(kNavigatedUrl, new_tab_observer.GetWebContents()->GetURL());
}

// If the user clicks 'leave site', the warning should re-appear when the user
// re-visits the page.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       LeaveSiteStillWarnsAfter) {
  auto kNavigatedUrl = GetURL("site1.com");
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningLeaveSite(browser());

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_TRUE(IsUIShowingIfEnabled());
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
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

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
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);

  EXPECT_FALSE(IsUIShowing());
  EXPECT_EQ(kNavigatedUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputationIgnored,
      GURL()));
}

// Non main-frame navigations should be ignored.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       IgnoreIFrameNavigations) {
  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
  const GURL kFrameUrl =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  SetSafetyTipBadRepPatterns({"a.com/"});

  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(IsUIShowingIfEnabled());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoShowsSafetyTipInfo(
      browser(), security_state::SafetyTipStatus::kBadReputation, GURL()));
}

// Background tabs shouldn't open a bubble initially, but should when they
// become visible.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       BubbleWaitsForVisible) {
  auto kFlaggedUrl = GetURL("site1.com");

  TriggerWarning(browser(), kFlaggedUrl,
                 WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_FALSE(IsUIShowing());

  auto* tab_strip = browser()->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->active_index() + 1);

  EXPECT_TRUE(IsUIShowingIfEnabled());
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
  EXPECT_TRUE(IsUIShowingOnlyIfFeaturesEnabled());

  if (ui_status() == UIStatus::kEnabledWithAllFeatures) {
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
  EXPECT_TRUE(IsUIShowingOnlyIfFeaturesEnabled());

  // ...but suppressed by the allowlist.
  SetSafetyTipAllowlistPatterns({"xn--googl-fsa.sk/"});
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());
}

// Disabled due to consistent failure: http://crbug.com/1020109
#if defined(OS_LINUX)
#define MAYBE_TriggersOnEditDistance DISABLED_TriggersOnEditDistance
#else
#define MAYBE_TriggersOnEditDistance TriggersOnEditDistance
#endif
// Tests that Safety Tips trigger (or not) on lookalike domains with edit
// distance when enabled, and not otherwise.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       MAYBE_TriggersOnEditDistance) {
  // This domain is an edit distance of one from the top 500.
  const GURL kNavigatedUrl = GetURL("goooglé.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(IsUIShowing(), ui_status() == UIStatus::kEnabledWithAllFeatures);
}

// Tests that the SafetyTipShown histogram triggers correctly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       SafetyTipShownHistogram) {
  const char kHistogramName[] = "Security.SafetyTips.SafetyTipShown";
  base::HistogramTester histograms;

  auto kNavigatedUrl = GetURL("site1.com");
  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kNone, 1);

  auto kBadRepUrl = GetURL("site2.com");
  TriggerWarning(browser(), kBadRepUrl, WindowOpenDisposition::CURRENT_TAB);
  CloseWarningLeaveSite(browser());
  histograms.ExpectBucketCount(
      kHistogramName, security_state::SafetyTipStatus::kBadReputation, 1);

  const GURL kLookalikeUrl = GetURL("googlé.sk");
  SetEngagementScore(browser(), kLookalikeUrl, kLowEngagement);
  NavigateToURL(browser(), kLookalikeUrl, WindowOpenDisposition::CURRENT_TAB);

  // Record metrics for lookalike domains unless explicitly disabled.
  histograms.ExpectBucketCount(kHistogramName,
                               security_state::SafetyTipStatus::kLookalike,
                               ui_status() == UIStatus::kEnabled ? 0 : 1);
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
  TriggerWarning(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  CloseWarningIgnore(views::Widget::ClosedReason::kCloseButtonClicked);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  histograms.ExpectBucketCount(
      "Security.SafetyTips.SafetyTipIgnoredPageLoad",
      security_state::SafetyTipStatus::kBadReputationIgnored, 1);
}

// Disabled due to consistent failure: http://crbug.com/1020109
#if defined(OS_LINUX)
#define MAYBE_InteractionsHistogram DISABLED_InteractionsHistogram
#else
#define MAYBE_InteractionsHistogram InteractionsHistogram
#endif
// Tests that Safety Tip interactions are recorded in a histogram.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       MAYBE_InteractionsHistogram) {
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
    TriggerWarning(browser(), kNavigatedUrl,
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
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    CloseWarningIgnore(views::Widget::ClosedReason::kEscKeyPressed);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismiss, 1);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismissWithEsc, 1);
  }

  {
    base::HistogramTester histogram_tester;
    auto kNavigatedUrl = GetURL("site3.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    CloseWarningIgnore(views::Widget::ClosedReason::kCancelButtonClicked);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismiss, 1);
    histogram_tester.ExpectBucketCount(
        kHistogramPrefix + "SafetyTip_BadReputation",
        SafetyTipInteraction::kDismissWithIgnore, 1);
  }
}

// Tests that the histograms recording how long the Safety Tip is open are
// recorded properly.
IN_PROC_BROWSER_TEST_P(SafetyTipPageInfoBubbleViewBrowserTest,
                       TimeOpenHistogram) {
  if (ui_status() == UIStatus::kDisabled) {
    return;
  }
  const base::TimeDelta kMinWarningTime = base::TimeDelta::FromMilliseconds(10);

  // Test the histogram for no user action taken.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    // Ensure that the tab is open for more than 0 ms, even in the face of bots
    // with bad clocks.
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
    NavigateToURL(browser(), GURL("about:blank"),
                  WindowOpenDisposition::CURRENT_TAB);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.NoAction.SafetyTip_BadReputation");
    ASSERT_EQ(1u, samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), samples.front().min);
  }

  // Test the histogram for when the user adheres to the warning.
  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site1.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
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
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
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
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
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

  {
    base::HistogramTester histograms;
    auto kNavigatedUrl = GetURL("site3.com");
    TriggerWarning(browser(), kNavigatedUrl,
                   WindowOpenDisposition::CURRENT_TAB);
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kMinWarningTime);
    run_loop.Run();
    CloseWarningIgnore(views::Widget::ClosedReason::kCancelButtonClicked);
    auto base_samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.Dismiss.SafetyTip_"
        "BadReputation");
    ASSERT_EQ(1u, base_samples.size());
    EXPECT_LE(kMinWarningTime.InMilliseconds(), base_samples.front().min);
    auto samples = histograms.GetAllSamples(
        "Security.SafetyTips.OpenTime.DismissWithIgnore.SafetyTip_"
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
  SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, chrome_browser_safety_tips::FlaggedPage::UNKNOWN);

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
  SetSafetyTipPatternsWithFlagType(
      {"site1.com/"}, chrome_browser_safety_tips::FlaggedPage::YOUNG_DOMAIN);

  SetEngagementScore(browser(), kNavigatedUrl, kLowEngagement);
  NavigateToURL(browser(), kNavigatedUrl, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(IsUIShowing());

  ASSERT_NO_FATAL_FAILURE(CheckPageInfoDoesNotShowSafetyTipInfo(browser()));
}
