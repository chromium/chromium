// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
const char kAppId1[] = "abcdefg";
}  // namespace

class IntentPickerBubbleViewBrowserTestChromeOS : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    app_service_test_.SetUp(browser()->profile());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(app_service_proxy_);
  }

  void AddFakeAppWithIntentFilter(const std::string& app_id,
                                  const std::string& app_name,
                                  const GURL& url,
                                  const apps::mojom::AppType app_type) {
    std::vector<apps::mojom::AppPtr> apps;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->app_type = app_type;
    app->name = app_name;
    auto intent_filter = apps_util::CreateIntentFilterForUrlScope(url);
    app->intent_filters.push_back(std::move(intent_filter));
    apps.push_back(std::move(app));
    app_service_proxy_->AppRegistryCache().OnApps(std::move(apps));
    app_service_test_.WaitForAppService();
  }

  PageActionIconView* GetIntentPickerIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

 private:
  apps::AppServiceProxy* app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

// Test that the intent picker bubble will pop out for ARC apps.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       BubblePopOut) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  AddFakeAppWithIntentFilter(kAppId1, app_name, test_url,
                             apps::mojom::AppType::kArc);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(kAppId1, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);
}

// Test that navigate outside url scope will not show the intent picker icon or
// bubble.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       OutOfScopeDoesNotShowBubble) {
  GURL test_url("https://www.google.com/");
  GURL out_of_scope_url("https://www.example.com/");
  std::string app_name = "test_name";
  AddFakeAppWithIntentFilter(kAppId1, app_name, test_url,
                             apps::mojom::AppType::kArc);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), out_of_scope_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_FALSE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());
}

// Test that intent picker bubble will not pop up for only PWA apps.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PWAOnlyDoesNotShowBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  AddFakeAppWithIntentFilter(kAppId1, app_name, test_url,
                             apps::mojom::AppType::kWeb);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());
}

// Test that intent picker bubble will not pop up for non-link navigation.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       NotLinkDoesNotShowBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  AddFakeAppWithIntentFilter(kAppId1, app_name, test_url,
                             apps::mojom::AppType::kArc);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());
}

// Test that dismiss the bubble for 2 times for the same origin will not show
// the bubble again. Test that the intent picker bubble will pop out for ARC
// apps.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       DismissBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  AddFakeAppWithIntentFilter(kAppId1, app_name, test_url,
                             apps::mojom::AppType::kArc);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(kAppId1, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);
  EXPECT_TRUE(intent_picker_bubble()->Close());

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_TRUE(intent_picker_bubble()->Close());

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ui_test_utils::NavigateToURL(&params);
  app_service_test().WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());
}
