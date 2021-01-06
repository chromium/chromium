// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_loader.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// A simple fake implementation of OneGoogleBarLoader that immediately returns
// a pre-configured OneGoogleBarData, which is null by default.
class FakeOneGoogleBarLoader : public OneGoogleBarLoader {
 public:
  void Load(OneGoogleCallback callback) override {
    std::move(callback).Run(Status::OK, one_google_bar_data_);
  }

  GURL GetLoadURLForTesting() const override { return GURL(); }

  bool SetAdditionalQueryParams(const std::string& value) override {
    return false;
  }

  void set_one_google_bar_data(
      const base::Optional<OneGoogleBarData>& one_google_bar_data) {
    one_google_bar_data_ = one_google_bar_data;
  }

 private:
  base::Optional<OneGoogleBarData> one_google_bar_data_;
};

class LocalNTPOneGoogleBarSmokeTest : public InProcessBrowserTest {
 public:
  LocalNTPOneGoogleBarSmokeTest() {}

  FakeOneGoogleBarLoader* one_google_bar_loader() {
    return static_cast<FakeOneGoogleBarLoader*>(
        OneGoogleBarServiceFactory::GetForProfile(browser()->profile())
            ->loader_for_testing());
  }

 private:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&LocalNTPOneGoogleBarSmokeTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateOneGoogleBarService(
      content::BrowserContext* context) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(
            Profile::FromBrowserContext(context));
    return std::make_unique<OneGoogleBarService>(
        identity_manager, std::make_unique<FakeOneGoogleBarLoader>());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    OneGoogleBarServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &LocalNTPOneGoogleBarSmokeTest::CreateOneGoogleBarService));
  }

  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPOneGoogleBarSmokeTest,
                       NTPLoadsWithoutErrorOnNetworkFailure) {
  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  content::WebContentsConsoleObserver console_observer(active_tab);

  // Navigate to the local NTP.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0u);
}

IN_PROC_BROWSER_TEST_F(LocalNTPOneGoogleBarSmokeTest,
                       NTPLoadsWithOneGoogleBar) {
  OneGoogleBarData data;
  data.bar_html = "<div id='thebar'></div>";
  data.in_head_script = "window.inHeadRan = true;";
  data.after_bar_script = "window.afterBarRan = true;";
  data.end_of_body_script = "console.log('ogb-done');";
  one_google_bar_loader()->set_one_google_bar_data(data);

  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for the "ogb-done" message, which
  // indicates that the OGB has finished loading.
  content::WebContentsConsoleObserver console_observer(active_tab);
  console_observer.SetPattern("ogb-done");

  // Navigate to the local NTP.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());
  // Make sure the OGB is finished loading.
  console_observer.Wait();

  EXPECT_EQ("ogb-done", console_observer.GetMessageAt(0u));

  bool in_head_ran = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.inHeadRan", &in_head_ran));
  EXPECT_TRUE(in_head_ran);
  bool after_bar_ran = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!window.afterBarRan", &after_bar_ran));
  EXPECT_TRUE(after_bar_ran);
}
