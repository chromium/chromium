// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsEmpty;

class MockPromoService : public PromoService {
 public:
  MockPromoService() : PromoService(nullptr, nullptr) {}

  void Refresh() override { PromoDataLoaded(promo_status_, promo_data_); }

  void SetupWithPromo(const PromoData& promo_data) {
    promo_data_ = promo_data;
    promo_status_ = Status::OK_WITH_PROMO;
  }

  void SetupWithoutPromo() { promo_status_ = Status::OK_WITHOUT_PROMO; }

  void SetupWithFailure() { promo_status_ = Status::FATAL_ERROR; }

  void SetupWithBlockedPromo() { promo_status_ = Status::OK_BUT_BLOCKED; }

 private:
  PromoData promo_data_;
  Status promo_status_;
};

class LocalNTPPromoTest : public InProcessBrowserTest {
 protected:
  MockPromoService* promo_service() {
    return static_cast<MockPromoService*>(
        PromoServiceFactory::GetForProfile(browser()->profile()));
  }

 private:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &LocalNTPPromoTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreatePromoService(
      content::BrowserContext* context) {
    return std::make_unique<MockPromoService>();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    PromoServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&LocalNTPPromoTest::CreatePromoService));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest, PromoInjectedIntoPage) {
  PromoData promo;
  promo.promo_html = "<div>promo</div>";
  promo_service()->SetupWithPromo(promo);

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('promo').innerHTML === '<div>promo</div>'", &result));
  ASSERT_TRUE(result);

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 1);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest, NoPromoAvailable) {
  promo_service()->SetupWithoutPromo();

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('promo') === null", &result));
  ASSERT_TRUE(result);

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 1);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest, PromoFetchFails) {
  promo_service()->SetupWithFailure();

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('promo') === null", &result));
  ASSERT_TRUE(result);

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 1);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest, BlockedPromoFetched) {
  promo_service()->SetupWithBlockedPromo();

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('promo') === null", &result));
  ASSERT_TRUE(result);

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 1);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 0);
}

// Tests are disabled until we implement a way to navigate to chrome scheme
// links from the NTP.
IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest,
                       DISABLED_PromoWithPriviligedLinkAndPermission) {
  PromoData promo;
  promo.promo_html = "<div><a href=\"chrome://extensions\">promo</a></div>";
  promo.can_open_privileged_links = true;
  promo_service()->SetupWithPromo(promo);

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  content::TestNavigationObserver nav_observer(active_tab, 1);
  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('promo').innerHTML === '<div><a "
      "href=\"chrome://extensions\">promo</a></div>'",
      &result));
  ASSERT_TRUE(result);

  // Click on privileged link
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "$('promo').querySelector('a').click()"));
  nav_observer.Wait();

  // Expect navigation to the chrome extensions page to succeed.
  EXPECT_EQ(active_tab->GetLastCommittedURL(), chrome::kChromeUIExtensionsURL);

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 1);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPPromoTest,
                       DISABLED_PromoWithPriviligedLinkNoPermission) {
  PromoData promo;
  promo.promo_html = "<div><a href=\"chrome://extensions\">promo</a></div>";
  promo_service()->SetupWithPromo(promo);

  base::HistogramTester histograms;

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);

  bool result;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('promo').innerHTML === '<div><a "
      "href=\"chrome://extensions\">promo</a></div>'",
      &result));
  ASSERT_TRUE(result);

  // Observe another roundtrip to the renderer process to ensure that the event
  // loop has been executed.
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  content::TitleWatcher title_watcher(active_tab, expected_title);
  // Click on a priviliged link.
  content::DidStartNavigationObserver did_start_navigation_observer(active_tab);
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "$('promo').querySelector('a').click()"));

  EXPECT_TRUE(ExecuteScript(active_tab, "document.title = 'loaded';"));
  // Make sure the event loop has been executed.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  // Expect that no navigation was observed.
  EXPECT_FALSE(did_start_navigation_observer.observed());

  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", 1);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", 0);
  histograms.ExpectTotalCount(
      "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.RequestLatency2.Failure", 0);
  histograms.ExpectTotalCount("NewTabPage.Promos.ShownTime", 1);
}
