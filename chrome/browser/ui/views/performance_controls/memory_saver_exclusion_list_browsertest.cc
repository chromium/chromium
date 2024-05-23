// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_browser_test_mixin.h"
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
const char kWWWExampleURL[] = "www.example.com";
const char kExampleURL[] = "example.com";
const char kSubDomainURL[] = "test.example.com";
const char kPathWithoutQuery[] = "/extensions/favicon/test_file.html";
const char kPathWithQuery[] =
    "/extensions/favicon/test_file.html?q=hello+world";
}  // namespace

class MemorySaverExclusionListBrowserTest
    : public MemorySaverBrowserTestMixin<InProcessBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    MemorySaverBrowserTestMixin::SetUpOnMainThread();

    GURL test_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    content::NavigateToURLBlockUntilNavigationsComplete(
        browser()->tab_strip_model()->GetActiveWebContents(), test_url, 1);
    ASSERT_TRUE(AddTabAtIndexToBrowser(
        browser(), 1, test_url, ui::PageTransition::PAGE_TRANSITION_LINK));
  }

  // Navigates the first tab to the given url and attempts to
  // discard that tab. Returns whether if the tab was successfully discarded
  bool NavigateAndDiscardFirstTab(GURL url) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    EXPECT_NE(tab_strip_model->active_index(), 0);
    content::NavigateToURLBlockUntilNavigationsComplete(
        tab_strip_model->GetWebContentsAt(0), url, 1);
    return TryDiscardTabAt(0);
  }
};

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesHost) {
  SetTabDiscardExceptionsMap({"example.com"});

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(base::ToUpperASCII(kExampleURL))));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesHostExactly) {
  SetTabDiscardExceptionsMap({".example.com"});

  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(base::ToUpperASCII(kExampleURL))));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesSubdirectories) {
  SetTabDiscardExceptionsMap({
      "example.com/extensions/favicon",
  });

  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithoutQuery)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesSchemeWildCards) {
  SetTabDiscardExceptionsMap({"http://*"});

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesQueryParamWildCards) {
  SetTabDiscardExceptionsMap(
      {"example.com/extensions/favicon/test_file.html?q=hello*"});

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesAllSitesWildCards) {
  SetTabDiscardExceptionsMap({"*"});

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kSubDomainURL)));
  EXPECT_FALSE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}

IN_PROC_BROWSER_TEST_F(MemorySaverExclusionListBrowserTest,
                       ExclusionListMatchesPort) {
  SetTabDiscardExceptionsMap(
      {std::string("example.com:") +
       base::NumberToString(embedded_test_server()->port())});

  EXPECT_FALSE(NavigateAndDiscardFirstTab(GetURL(kExampleURL)));
  EXPECT_FALSE(
      NavigateAndDiscardFirstTab(GetURL(kWWWExampleURL, kPathWithQuery)));
  EXPECT_TRUE(NavigateAndDiscardFirstTab(GURL(chrome::kChromeUISettingsURL)));
}
