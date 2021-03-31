// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace {
const char kTestAppActivity[] = "abcdefg";

class FakeIconLoader : public apps::IconLoader {
 public:
  FakeIconLoader() = default;
  FakeIconLoader(const FakeIconLoader&) = delete;
  FakeIconLoader& operator=(const FakeIconLoader&) = delete;
  ~FakeIconLoader() override = default;

  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override {
    return apps::mojom::IconKey::New(0, 0, 0);
  }

  std::unique_ptr<apps::IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override {
    auto iv = apps::mojom::IconValue::New();
    iv->icon_type = icon_type;
    iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
    iv->is_placeholder_icon = false;

    std::move(callback).Run(std::move(iv));
    return nullptr;
  }
};
}  // namespace

class IntentPickerBubbleViewBrowserTestChromeOS : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile());
    ASSERT_TRUE(app_service_proxy_);
    app_service_proxy_->OverrideInnerIconLoaderForTesting(&icon_loader_);

    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    intent_helper_instance_ = std::make_unique<arc::FakeIntentHelperInstance>();
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(intent_helper_instance_.get());
    WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper());
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host());
    arc::ArcServiceManager::Get()->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->app());
  }

  std::string AddArcAppWithIntentFilter(const std::string& app_name,
                                        const GURL& url) {
    std::vector<arc::mojom::AppInfoPtr> app_infos;

    arc::mojom::AppInfoPtr app_info(arc::mojom::AppInfo::New());
    app_info->name = app_name;
    app_info->package_name = app_name;
    app_info->activity = kTestAppActivity;
    app_info->sticky = false;
    app_infos.push_back(std::move(app_info));
    app_host()->OnAppListRefreshed(std::move(app_infos));
    WaitForAppService();
    std::string app_id = ArcAppListPrefs::GetAppId(app_name, kTestAppActivity);
    auto test_app_info = app_prefs()->GetApp(app_id);
    EXPECT_TRUE(test_app_info);

    std::vector<apps::mojom::AppPtr> apps;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->app_type = apps::mojom::AppType::kArc;
    app->name = app_name;
    auto intent_filter = apps_util::CreateIntentFilterForUrlScope(url);
    app->intent_filters.push_back(std::move(intent_filter));
    apps.push_back(std::move(app));
    app_service_proxy_->AppRegistryCache().OnApps(
        std::move(apps), apps::mojom::AppType::kArc,
        false /* should_notify_initialized */);
    WaitForAppService();

    return app_id;
  }

  std::string InstallWebApp(const std::string& app_name, const GURL& url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = url;
    web_app_info->scope = url;
    web_app_info->open_as_window = true;
    auto app_id = web_app::InstallWebApp(profile(), std::move(web_app_info));
    WaitForAppService();
    return app_id;
  }

  PageActionIconView* GetIntentPickerIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  void WaitForAppService() { base::RunLoop().RunUntilIdle(); }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

  Profile* profile() { return browser()->profile(); }

  // The handled intents list in the intent helper instance represents the arc
  // app that app service tried to launch.
  const std::vector<arc::FakeIntentHelperInstance::HandledIntent>&
  launched_arc_apps() {
    return intent_helper_instance_->handled_intents();
  }

  void ClickIconToShowBubble() {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "IntentPickerBubbleView");
    GetIntentPickerIcon()->ExecuteForTesting();
    waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(intent_picker_bubble());
    EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  }

  // Dummy method to be called upon bubble closing.
  void OnBubbleClosed(const std::string& selected_app_package,
                      apps::PickerEntryType entry_type,
                      apps::IntentPickerCloseReason close_reason,
                      bool should_persist) {
    bubble_closed_ = true;
  }

  void ShowBubbleForTesting() {
    std::vector<apps::IntentPickerAppInfo> app_info;
    app_info.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                          "package_1", "dank app 1");
    app_info.emplace_back(apps::PickerEntryType::kArc, ui::ImageModel(),
                          "package_2", "dank_app_2");

    browser()->window()->ShowIntentPickerBubble(
        std::move(app_info), /*show_stay_in_chrome=*/true,
        /*show_remember_selection=*/true, PageActionIconType::kIntentPicker,
        base::nullopt,
        base::BindOnce(
            &IntentPickerBubbleViewBrowserTestChromeOS::OnBubbleClosed,
            base::Unretained(this)));
  }

  bool bubble_closed() { return bubble_closed_; }

 private:
  apps::AppServiceProxyChromeOs* app_service_proxy_ = nullptr;
  std::unique_ptr<arc::FakeIntentHelperInstance> intent_helper_instance_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  FakeIconLoader icon_loader_;
  bool bubble_closed_ = false;
};

// Test that the intent picker bubble will pop out for ARC apps.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       BubblePopOut) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  ASSERT_EQ(1U, launched_arc_apps().size());
  EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
  EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
}

// Test that navigate outside url scope will not show the intent picker icon or
// bubble.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       OutOfScopeDoesNotShowBubble) {
  GURL test_url("https://www.google.com/");
  GURL out_of_scope_url("https://www.example.com/");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), out_of_scope_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_FALSE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());
}

// Test that intent picker bubble pop up status will depends on
// kIntentPickerPWAPersistence flag for if there is only PWA as
// candidates.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PWAOnlyShowBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  auto app_id = InstallWebApp(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  if (base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence)) {
    ASSERT_TRUE(intent_picker_bubble());
    EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  } else {
    EXPECT_FALSE(intent_picker_bubble());
    ClickIconToShowBubble();
  }

  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the app.
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
}

// Test that intent picker bubble will not pop up for non-link navigation.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       NotLinkDoesNotShowBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());

  ClickIconToShowBubble();
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  ASSERT_EQ(1U, launched_arc_apps().size());
  EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
  EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
}

// Test that dismiss the bubble for 2 times for the same origin will not show
// the bubble again. Test that the intent picker bubble will pop out for ARC
// apps.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       DismissBubble) {
  GURL test_url("https://www.google.com/");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);
  EXPECT_TRUE(intent_picker_bubble()->Close());

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_TRUE(intent_picker_bubble()->Close());

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ui_test_utils::NavigateToURL(&params);
  WaitForAppService();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());

  ClickIconToShowBubble();
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& new_app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, new_app_info.size());
  EXPECT_EQ(app_id, new_app_info[0].launch_name);
  EXPECT_EQ(app_name, new_app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  ASSERT_EQ(1U, launched_arc_apps().size());
  EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
  EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
}

// Test that show intent picker bubble twice without closing doesn't
// crash the browser.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ShowBubbleTwice) {
  ShowBubbleForTesting();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(2U, intent_picker_bubble()->GetScrollViewSize());
  ShowBubbleForTesting();
  ASSERT_TRUE(bubble_closed());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(2U, intent_picker_bubble()->GetScrollViewSize());
}

// Test that loading a page with pushState() call that doesn't change URL work
// as normal.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PushStateLoadingTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_url =
      embedded_test_server()->GetURL("/intent_picker/push_state_test.html");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  ASSERT_EQ(1U, launched_arc_apps().size());
  EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
  EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
}

// Test that loading a page with pushState() call that changes URL
// updates the intent picker view.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PushStateURLChangeTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_url =
      embedded_test_server()->GetURL("/intent_picker/push_state_test.html");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(intent_picker_view->GetVisible());
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);
  EXPECT_TRUE(intent_picker_bubble()->Close());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  SimulateMouseClickOrTapElementWithId(web_contents, "push_to_new_url_button");
  observer.WaitForNavigationFinished();
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that reload a page after app installation will show intent picker.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ReloadAfterInstall) {
  GURL test_url("https://www.google.com/");
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Navigate from a link.
  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  WaitForAppService();
  EXPECT_FALSE(intent_picker_view->GetVisible());

  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);

  // Reload the page and the intent picker should show up.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.WaitForNavigationFinished();

  EXPECT_TRUE(intent_picker_view->GetVisible());

  ClickIconToShowBubble();
  EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();
  ASSERT_EQ(1U, launched_arc_apps().size());
  EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
  EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
}
