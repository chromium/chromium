// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
constexpr base::TimeDelta kShortDelay = base::Seconds(1);
const char kWWWExampleURL[] = "www.example.com";
const char kExampleURL[] = "example.com";
const char kSubDomainURL[] = "test.example.com";
const char kPathWithoutQuery[] = "/extensions/favicon/test_file.html";
const char kPathWithQuery[] =
    "/extensions/favicon/test_file.html?q=hello+world";
}  // namespace

class HighEfficiencyExclusionListBrowserTest : public InProcessBrowserTest {
 public:
  HighEfficiencyExclusionListBrowserTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  ~HighEfficiencyExclusionListBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "1h"}}}},
        {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    TabStripModel* tab_strip_model = browser()->tab_strip_model();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(tab_strip_model);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    content::NavigateToURLBlockUntilNavigationsComplete(
        tab_strip_model->GetActiveWebContents(), test_url, 1);
    ASSERT_TRUE(AddTabAtIndexToBrowser(
        browser(), 1, test_url, ui::PageTransition::PAGE_TRANSITION_LINK));
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  bool TryDiscardTabAt(int tab_index) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    performance_manager::user_tuning::UserPerformanceTuningManager* manager =
        performance_manager::user_tuning::UserPerformanceTuningManager::
            GetInstance();
    manager->DiscardPageForTesting(
        tab_strip_model->GetWebContentsAt(tab_index));

    return tab_strip_model->GetWebContentsAt(tab_index)->WasDiscarded();
  }

  PrefService* GetPrefs() { return browser()->profile()->GetPrefs(); }

  // Navigates the first tab to the given url and attempts to
  // discard that tab. Returns whether if the tab was successfully discarded
  bool NavigateAndDiscardFirstTab(GURL url) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_NE(tab_strip_model->active_index(), 0);
    content::NavigateToURLBlockUntilNavigationsComplete(
        tab_strip_model->GetWebContentsAt(0), url, 1);
    return TryDiscardTabAt(0);
  }

  GURL GetURL(base::StringPiece host, base::StringPiece path = "/title1.html") {
    return embedded_test_server()->GetURL(host, path);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
};

IN_PROC_BROWSER_TEST_F(HighEfficiencyExclusionListBrowserTest,
                       ExclusionListMatchesHost) {
  base::Value::List exclusion_list;
  exclusion_list.Append("example.com");
  GetPrefs()->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclusion_list));

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(base::ToUpperASCII(kExampleURL))));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyExclusionListBrowserTest,
                       ExclusionListMatchesHostExactly) {
  base::Value::List exclusion_list;
  exclusion_list.Append(".example.com");
  GetPrefs()->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclusion_list));

  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(base::ToUpperASCII(kExampleURL))));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyExclusionListBrowserTest,
                       ExclusionListMatchesSubdirectories) {
  base::Value::List exclusion_list;
  exclusion_list.Append("example.com/extensions/favicon");
  GetPrefs()->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclusion_list));

  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithoutQuery)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyExclusionListBrowserTest,
                       ExclusionListMatchesWildCards) {
  PrefService* prefs = GetPrefs();
  base::Value::List exclude_sites_with_http_scheme;
  exclude_sites_with_http_scheme.Append("http://*");
  prefs->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclude_sites_with_http_scheme));

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));

  base::Value::List wildcard_at_end_of_query;
  wildcard_at_end_of_query.Append(
      "example.com/extensions/favicon/test_file.html?q=hello*");
  prefs->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(wildcard_at_end_of_query));

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));

  base::Value::List exclude_all_sites;
  exclude_all_sites.Append("*");
  prefs->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclude_all_sites));

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}

IN_PROC_BROWSER_TEST_F(HighEfficiencyExclusionListBrowserTest,
                       ExclusionListMatchesPort) {
  base::Value::List exclusion_list;
  exclusion_list.Append(std::string("example.com:") +
                        base::NumberToString(embedded_test_server()->port()));
  GetPrefs()->SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclusion_list));

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}
