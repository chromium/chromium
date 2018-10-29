// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/infobars/infobar_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/lookalike_url_navigation_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/window_open_disposition.h"

namespace {

using UkmEntry = ukm::builders::LookalikeUrl_NavigationSuggestion;

enum class FeatureTestState { kDisabled, kEnabledWithoutUI, kEnabledWithUI };

struct SiteEngagementTestCase {
  const char* const navigated;
  const char* const suggested;
} kSiteEngagementTestCases[] = {
    {"sité1.test", "site1.test"},
    {"mail.www.sité1.test", "site1.test"},

    // These should match since the comparison uses eTLD+1s.
    {"sité2.test", "www.site2.test"},
    {"mail.sité2.test", "www.site2.test"},

    {"síté3.test", "sité3.test"},
    {"mail.síté3.test", "sité3.test"},

    {"síté4.test", "www.sité4.test"},
    {"mail.síté4.test", "www.sité4.test"},
};

}  // namespace

class LookalikeUrlNavigationObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<FeatureTestState> {
 protected:
  void SetUp() override {
    if (GetParam() == FeatureTestState::kEnabledWithoutUI) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kLookalikeUrlNavigationSuggestions,
          {{"metrics_only", "true"}});
    } else if (GetParam() == FeatureTestState::kEnabledWithUI) {
      feature_list_.InitAndEnableFeature(
          features::kLookalikeUrlNavigationSuggestions);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kLookalikeUrlNavigationSuggestions);
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Sets the absolute Site Engagement |score| for the testing origin.
  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService::Get(browser()->profile())
        ->ResetBaseScoreForURL(url, score);
  }

  // Simulates a link click navigation. We don't use
  // ui_test_utils::NavigateToURL(const GURL&) because it simulates the user
  // typing the URL, causing the site to have a site engagement score of at
  // least LOW.
  void NavigateToURL(const GURL& url) {
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    params.is_renderer_initiated = true;
    ui_test_utils::NavigateToURL(&params);
  }

  // Checks that UKM recorded a metric for each URL in |navigated_urls|.
  void CheckUkm(const std::vector<GURL>& navigated_urls,
                LookalikeUrlNavigationObserver::MatchType match_type) {
    auto entries = test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(navigated_urls.size(), entries.size());
    int entry_count = 0;
    for (const auto* const entry : entries) {
      test_ukm_recorder()->ExpectEntrySourceHasUrl(entry,
                                                   navigated_urls[entry_count]);
      test_ukm_recorder()->ExpectEntryMetric(entry, "MatchType",
                                             static_cast<int>(match_type));
      entry_count++;
    }
  }

  void CheckNoUkm() {
    EXPECT_TRUE(
        test_ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName).empty());
  }

  void TestInfobarNotShown(const GURL& navigated_url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);

    content::TestNavigationObserver navigation_observer(web_contents, 1);
    NavigateToURL(navigated_url);
    navigation_observer.Wait();
    EXPECT_EQ(0u, infobar_service->infobar_count());
  }

  void TestInfobarShown(const GURL& navigated_url,
                        const GURL& expected_suggested_url) {
    // Sanity check navigated_url.
    url_formatter::IDNConversionResult result =
        url_formatter::IDNToUnicodeWithDetails(navigated_url.host_piece());
    ASSERT_TRUE(result.has_idn_component);

    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    InfoBarObserver infobar_added_observer(
        infobar_service, InfoBarObserver::Type::kInfoBarAdded);
    NavigateToURL(navigated_url);
    infobar_added_observer.Wait();

    infobars::InfoBar* infobar = infobar_service->infobar_at(0);
    EXPECT_EQ(infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
              infobar->delegate()->GetIdentifier());

    // Clicking the link in the infobar should remove the infobar and navigate
    // to the suggested URL.
    InfoBarObserver infobar_removed_observer(
        infobar_service, InfoBarObserver::Type::kInfoBarRemoved);
    AlternateNavInfoBarDelegate* infobar_delegate =
        static_cast<AlternateNavInfoBarDelegate*>(infobar->delegate());
    infobar_delegate->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
    infobar_removed_observer.Wait();

    EXPECT_EQ(0u, infobar_service->infobar_count());
    EXPECT_EQ(expected_suggested_url, web_contents->GetURL());

    // Clicking the link in the infobar should also remove the original URL from
    // history.
    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    EXPECT_FALSE(base::ContainsValue(enumerator.urls(), navigated_url));
  }

  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

INSTANTIATE_TEST_CASE_P(,
                        LookalikeUrlNavigationObserverBrowserTest,
                        ::testing::Values(FeatureTestState::kDisabled,
                                          FeatureTestState::kEnabledWithoutUI,
                                          FeatureTestState::kEnabledWithUI));

// Navigating to a non-IDN shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       NonIdn_NoInfobar) {
  TestInfobarNotShown(
      embedded_test_server()->GetURL("google.com", "/title1.html"));
  CheckNoUkm();
}

// Navigating to a domain whose visual representation does not look like a
// top domain shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       NonTopDomainIdn_NoInfobar) {
  TestInfobarNotShown(
      embedded_test_server()->GetURL("éxample.com", "/title1.html"));
  CheckNoUkm();
}

// Navigating to a domain whose visual representation looks like a top domain
// should show a "Did you mean to go to ..." infobar and record metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       TopDomainIdn_Infobar) {
  if (GetParam() != FeatureTestState::kEnabledWithUI)
    return;

  base::HistogramTester histograms;

  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("googlé.com", "/title1.html");

  TestInfobarShown(kNavigatedUrl,
                   embedded_test_server()->GetURL(
                       "google.com", "/title1.html") /* suggested */);

  histograms.ExpectTotalCount(LookalikeUrlNavigationObserver::kHistogramName,
                              3);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationObserver::kHistogramName,
      LookalikeUrlNavigationObserver::NavigationSuggestionEvent::kInfobarShown,
      1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationObserver::kHistogramName,
      LookalikeUrlNavigationObserver::NavigationSuggestionEvent::kLinkClicked,
      1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationObserver::kHistogramName,
      LookalikeUrlNavigationObserver::NavigationSuggestionEvent::kMatchTopSite,
      1);
  CheckUkm({kNavigatedUrl},
           LookalikeUrlNavigationObserver::MatchType::kTopSite);
}

// Same as TopDomainIdn_Infobar but the UI is disabled, so only checks for
// metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       TopDomainIdn_Metrics_NoInfobar) {
  if (GetParam() != FeatureTestState::kEnabledWithoutUI)
    return;

  base::HistogramTester histograms;
  const GURL kNavigatedUrl =
      embedded_test_server()->GetURL("googlé.com", "/title1.html");

  TestInfobarNotShown(kNavigatedUrl);

  histograms.ExpectTotalCount(LookalikeUrlNavigationObserver::kHistogramName,
                              1);
  histograms.ExpectBucketCount(
      LookalikeUrlNavigationObserver::kHistogramName,
      LookalikeUrlNavigationObserver::NavigationSuggestionEvent::kMatchTopSite,
      1);
  CheckUkm({kNavigatedUrl},
           LookalikeUrlNavigationObserver::MatchType::kTopSite);
}

// Same as TopDomainIdn_Infobar but the user has engaged with the domain before.
// Shouldn't show an infobar.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       TopDomainIdn_EngagedSite_NoInfobar) {
  // If the user already engaged with the site, the infobar shouldn't be shown.
  const GURL url = embedded_test_server()->GetURL("googlé.com", "/title1.html");
  SetSiteEngagementScore(url, 20);
  TestInfobarNotShown(url);
  CheckNoUkm();
}

// Navigating to a domain whose visual representation looks like a domain with a
// site engagement score above a certain threshold should show a "Did you mean
// to go to ..." infobar.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       SiteEngagement_Infobar) {
  if (GetParam() != FeatureTestState::kEnabledWithUI)
    return;

  SetSiteEngagementScore(GURL("http://site1.test"), 20);
  SetSiteEngagementScore(GURL("http://www.site2.test"), 20);
  SetSiteEngagementScore(GURL("http://sité3.test"), 20);
  SetSiteEngagementScore(GURL("http://www.sité4.test"), 20);

  std::vector<GURL> ukm_urls;
  for (const auto& test_case : kSiteEngagementTestCases) {
    base::HistogramTester histograms;
    const GURL kNavigatedUrl =
        embedded_test_server()->GetURL(test_case.navigated, "/title1.html");
    TestInfobarShown(kNavigatedUrl, embedded_test_server()->GetURL(
                                        test_case.suggested, "/title1.html"));
    ukm_urls.push_back(kNavigatedUrl);

    histograms.ExpectTotalCount(LookalikeUrlNavigationObserver::kHistogramName,
                                3);
    histograms.ExpectBucketCount(LookalikeUrlNavigationObserver::kHistogramName,
                                 LookalikeUrlNavigationObserver::
                                     NavigationSuggestionEvent::kInfobarShown,
                                 1);
    histograms.ExpectBucketCount(
        LookalikeUrlNavigationObserver::kHistogramName,
        LookalikeUrlNavigationObserver::NavigationSuggestionEvent::kLinkClicked,
        1);
    histograms.ExpectBucketCount(
        LookalikeUrlNavigationObserver::kHistogramName,
        LookalikeUrlNavigationObserver::NavigationSuggestionEvent::
            kMatchSiteEngagement,
        1);
  }
  CheckUkm(ukm_urls,
           LookalikeUrlNavigationObserver::MatchType::kSiteEngagement);
}

// Same as SiteEngagement_Infobar but the UI is disabled, so only checks for
// metrics.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       SiteEngagement_Metrics_NoInfobar) {
  if (GetParam() != FeatureTestState::kEnabledWithoutUI)
    return;

  SetSiteEngagementScore(GURL("http://site1.test"), 20);
  SetSiteEngagementScore(GURL("http://www.site2.test"), 20);
  SetSiteEngagementScore(GURL("http://sité3.test"), 20);
  SetSiteEngagementScore(GURL("http://www.sité4.test"), 20);

  std::vector<GURL> ukm_urls;
  for (const auto& test_case : kSiteEngagementTestCases) {
    base::HistogramTester histograms;
    const GURL kNavigatedUrl =
        embedded_test_server()->GetURL(test_case.navigated, "/title1.html");
    TestInfobarNotShown(kNavigatedUrl);
    ukm_urls.push_back(kNavigatedUrl);

    histograms.ExpectTotalCount(LookalikeUrlNavigationObserver::kHistogramName,
                                1);
    histograms.ExpectBucketCount(
        LookalikeUrlNavigationObserver::kHistogramName,
        LookalikeUrlNavigationObserver::NavigationSuggestionEvent::
            kMatchSiteEngagement,
        1);
  }
  CheckUkm(ukm_urls,
           LookalikeUrlNavigationObserver::MatchType::kSiteEngagement);
}

// The infobar shouldn't be shown when the feature is disabled.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       TopDomainIdn_FeatureDisabled) {
  if (GetParam() != FeatureTestState::kDisabled)
    return;

  TestInfobarNotShown(
      embedded_test_server()->GetURL("googlé.com", "/title1.html"));
  CheckNoUkm();
}

// IDNs with a single label should be properly handled. There are two cases
// where this might occur:
// 1. The navigated URL is an IDN with a single label.
// 2. One of the engaged sites is an IDN with a single label.
// Neither of these should cause a crash.
IN_PROC_BROWSER_TEST_P(LookalikeUrlNavigationObserverBrowserTest,
                       IdnWithSingleLabelShouldNotCauseACrash) {
  if (GetParam() != FeatureTestState::kEnabledWithUI)
    return;

  base::HistogramTester histograms;

  // Case 1: Navigating to an IDN with a single label shouldn't cause a crash.
  TestInfobarNotShown(embedded_test_server()->GetURL("é", "/title1.html"));

  // Case 2: An IDN with a single label with a site engagement score shouldn't
  // cause a crash.
  SetSiteEngagementScore(GURL("http://tést"), 20);
  TestInfobarNotShown(
      embedded_test_server()->GetURL("tést.com", "/title1.html"));

  histograms.ExpectTotalCount(LookalikeUrlNavigationObserver::kHistogramName,
                              0);
  CheckNoUkm();
}
