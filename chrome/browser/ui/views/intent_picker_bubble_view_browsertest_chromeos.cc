// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kTestAppActivity[] = "abcdefg";

class FakeIconLoader : public apps::IconLoader {
 public:
  FakeIconLoader() = default;
  FakeIconLoader(const FakeIconLoader&) = delete;
  FakeIconLoader& operator=(const FakeIconLoader&) = delete;
  ~FakeIconLoader() override = default;

  std::unique_ptr<apps::IconLoader::Releaser> LoadIconFromIconKey(
      const std::string& id,
      const apps::IconKey& icon_key,
      apps::IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override {
    auto iv = std::make_unique<apps::IconValue>();
    iv->icon_type = icon_type;
    iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
    iv->is_placeholder_icon = false;

    std::move(callback).Run(std::move(iv));
    return nullptr;
  }
};

// Waits for a particular widget to be destroyed.
class WidgetDestroyedWaiter : public views::WidgetObserver {
 public:
  explicit WidgetDestroyedWaiter(views::Widget* widget) {
    observation_.Observe(widget);
  }
  ~WidgetDestroyedWaiter() override = default;

  // Blocks until OnWidgetDestroyed is called for the widget.
  void WaitForDestroyed() { run_loop_.Run(); }

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    run_loop_.Quit();
    observation_.Reset();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

class IntentPickerBubbleViewBrowserTestChromeOS : public InProcessBrowserTest {
 public:
  IntentPickerBubbleViewBrowserTestChromeOS() {
    // TODO(crbug.com/40236806): Run relevant tests against the updated UI.
    feature_list_.InitAndDisableFeature(apps::features::kLinkCapturingUiUpdate);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

    ASSERT_TRUE(embedded_test_server()->Start());
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
    std::string app_id = ArcAppListPrefs::GetAppId(app_name, kTestAppActivity);
    auto test_app_info = app_prefs()->GetApp(app_id);
    EXPECT_TRUE(test_app_info);

    auto app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    app->name = app_name;
    app->intent_filters.push_back(apps_util::MakeIntentFilterForUrlScope(url));
    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));
    app_service_proxy_->OnApps(std::move(apps), apps::AppType::kArc,
                               false /* should_notify_initialized */);
    return app_id;
  }

  std::string InstallWebApp(const std::string& app_name, const GURL& url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->scope = url;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    auto app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
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

  views::Checkbox* remember_selection_checkbox() {
    return views::AsViewClass<views::Checkbox>(
        intent_picker_bubble()->GetViewByID(
            IntentPickerBubbleView::ViewId::kRememberCheckbox));
  }

  // TODO(crbug.com/40203946): There should be an explicit signal we can wait on
  // rather than assuming the AppService will be started after RunUntilIdle.
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

  void clear_launched_arc_apps() {
    intent_helper_instance_->clear_handled_intents();
  }

  void ClickIconToShowBubble() {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        IntentPickerBubbleView::kViewClassName);
    GetIntentPickerIcon()->ExecuteForTesting();
    waiter.WaitIfNeededAndGet();
    ASSERT_TRUE(intent_picker_bubble());
    EXPECT_TRUE(intent_picker_bubble()->GetVisible());
  }

  void NavigateAndWaitForIconUpdate(const GURL& url) {
    DoAndWaitForIntentPickerIconUpdate([this, url]() {
      NavigateParams params(browser(), url,
                            ui::PageTransition::PAGE_TRANSITION_LINK);
      ui_test_utils::NavigateToURL(&params);
    });
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
        /*show_remember_selection=*/true,
        IntentPickerBubbleView::BubbleType::kLinkCapturing, std::nullopt,
        base::BindOnce(
            &IntentPickerBubbleViewBrowserTestChromeOS::OnBubbleClosed,
            base::Unretained(this)));
  }

  bool bubble_closed() { return bubble_closed_; }

  void CheckStayInChrome() {
    ASSERT_TRUE(intent_picker_bubble());
    intent_picker_bubble()->CancelDialog();
    EXPECT_EQ(BrowserList::GetInstance()->GetLastActive(), browser());
    EXPECT_EQ(launched_arc_apps().size(), 0U);
  }

  void VerifyArcAppLaunched(const std::string& app_name, const GURL& test_url) {
    WaitForAppService();
    ASSERT_EQ(1U, launched_arc_apps().size());
    EXPECT_EQ(app_name, launched_arc_apps()[0].activity->package_name);
    EXPECT_EQ(test_url.spec(), launched_arc_apps()[0].intent->data);
  }

  bool VerifyPWALaunched(const std::string& app_id) {
    WaitForAppService();
    Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
    return web_app::AppBrowserController::IsForWebApp(app_browser, app_id);
  }

  size_t GetItemContainerSize(IntentPickerBubbleView* bubble) {
    return bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

  views::View* GetButtonAtIndex(IntentPickerBubbleView* bubble, size_t index) {
    auto children =
        bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
            ->children();
    CHECK_LT(index, children.size());
    return children[index];
  }

  GURL InScopeAppUrl() {
    return embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  }

  GURL OutOfScopeAppUrl() {
    return embedded_test_server()->GetURL("/web_apps/minimal_ui/basic.html");
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  template <typename Action>
  void DoAndWaitForIntentPickerIconUpdate(Action action) {
    base::RunLoop run_loop;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    action();
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_ =
      nullptr;
  std::unique_ptr<arc::FakeIntentHelperInstance> intent_helper_instance_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  FakeIconLoader icon_loader_;
  bool bubble_closed_ = false;
};

// Test that the intent picker bubble will show for ARC apps.
//
// TODO(crbug.com/40863954): Fix timeouts under MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ArcOnlyShowBubble Disabled_ArcOnlyShowBubble
#else
#define MAYBE_ArcOnlyShowBubble ArcOnlyShowBubble
#endif
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       MAYBE_ArcOnlyShowBubble) {
  GURL test_url(InScopeAppUrl());
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Check the status of the remember selection checkbox.
  ASSERT_TRUE(remember_selection_checkbox());
  EXPECT_TRUE(remember_selection_checkbox()->GetEnabled());
  EXPECT_FALSE(remember_selection_checkbox()->GetChecked());

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());

  DoAndWaitForIntentPickerIconUpdate([this, app_name, test_url] {
    intent_picker_bubble()->AcceptDialog();
    ASSERT_NO_FATAL_FAILURE(VerifyArcAppLaunched(app_name, test_url));
  });

  // Make sure that the intent picker icon is no longer visible.
  ASSERT_TRUE(intent_picker_view);
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Test that intent picker bubble shows if there is only PWA as candidates.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PWAOnlyShowBubble) {
  GURL test_url(InScopeAppUrl());
  std::string app_name = "test_name";
  auto app_id = InstallWebApp(app_name, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Check the status of the remember selection checkbox.
  ASSERT_TRUE(remember_selection_checkbox());
  EXPECT_TRUE(remember_selection_checkbox()->GetEnabled());
  EXPECT_FALSE(remember_selection_checkbox()->GetChecked());

  // Launch the app.
  intent_picker_bubble()->AcceptDialog();
  EXPECT_TRUE(VerifyPWALaunched(app_id));
}

// Test that show intent picker bubble multiple times without closing doesn't
// crash the browser.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ShowBubbleMultipleTimes) {
  ShowBubbleForTesting();
  auto* bubble_1 = intent_picker_bubble();
  ASSERT_TRUE(bubble_1);
  EXPECT_TRUE(bubble_1->GetVisible());
  EXPECT_EQ(2U, GetItemContainerSize(intent_picker_bubble()));

  WidgetDestroyedWaiter bubble_1_waiter(bubble_1->GetWidget());

  ShowBubbleForTesting();
  auto* bubble_2 = intent_picker_bubble();
  ASSERT_TRUE(bubble_2);
  EXPECT_TRUE(bubble_2->GetVisible());
  EXPECT_EQ(2U, GetItemContainerSize(intent_picker_bubble()));
  // Bubble 1 should be fully destroyed after the second bubble appears.
  bubble_1_waiter.WaitForDestroyed();

  WidgetDestroyedWaiter bubble_2_waiter(bubble_2->GetWidget());

  ShowBubbleForTesting();
  auto* bubble_3 = intent_picker_bubble();
  ASSERT_TRUE(bubble_3);
  EXPECT_TRUE(bubble_3->GetVisible());
  EXPECT_EQ(2U, GetItemContainerSize(intent_picker_bubble()));
  // Bubble 2 should be fully destroyed after the third bubble appears.
  bubble_2_waiter.WaitForDestroyed();
}

// Test that loading a page with pushState() call that doesn't change URL work
// as normal.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       PushStateLoadingTest) {
  const GURL test_url =
      embedded_test_server()->GetURL("/intent_picker/push_state_test.html");
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  ASSERT_NO_FATAL_FAILURE(VerifyArcAppLaunched(app_name, test_url));
}

// Test that reload a page after app installation will show intent picker.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ReloadAfterInstall) {
  GURL test_url(InScopeAppUrl());
  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  EXPECT_FALSE(intent_picker_view->GetVisible());

  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);

  // Reload the page and the intent picker should show up.
  DoAndWaitForIntentPickerIconUpdate([this] {
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  });

  EXPECT_TRUE(intent_picker_view->GetVisible());

  ClickIconToShowBubble();
  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(app_id, app_info[0].launch_name);
  EXPECT_EQ(app_name, app_info[0].display_name);

  // Launch the default selected app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  ASSERT_NO_FATAL_FAILURE(VerifyArcAppLaunched(app_name, test_url));
}

// Test that stay in chrome works when there is only PWA candidates.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       StayInChromePWAOnly) {
  GURL test_url(InScopeAppUrl());
  std::string app_name = "test_name";
  auto app_id = InstallWebApp(app_name, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  ASSERT_NO_FATAL_FAILURE(CheckStayInChrome());
}

// Test that stay in chrome works when there is only ARC candidates.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       StayInChromeARCOnly) {
  GURL test_url(InScopeAppUrl());
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  ASSERT_NO_FATAL_FAILURE(CheckStayInChrome());
}

// Test that bubble pops out when there is both PWA and ARC candidates, and
// test launch the PWA.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ARCAndPWACandidateLaunchPWA) {
  base::HistogramTester histogram_tester;

  GURL test_url(InScopeAppUrl());
  std::string app_name_pwa = "pwa_test_name";
  auto app_id_pwa = InstallWebApp(app_name_pwa, test_url);
  std::string app_name_arc = "arc_test_name";
  auto app_id_arc = AddArcAppWithIntentFilter(app_name_arc, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  EXPECT_EQ(2U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(2U, app_info.size());
  const apps::IntentPickerAppInfo* pwa_app_info;
  const apps::IntentPickerAppInfo* arc_app_info;
  if (app_info[0].launch_name == app_id_pwa) {
    pwa_app_info = &app_info[0];
    arc_app_info = &app_info[1];
  } else {
    pwa_app_info = &app_info[1];
    arc_app_info = &app_info[0];

    // Select the PWA when it is not automatically selected.
    auto event_generator = ui::test::EventGenerator(
        views::GetRootWindow(intent_picker_bubble()->GetWidget()));
    event_generator.MoveMouseTo(GetButtonAtIndex(intent_picker_bubble(), 1)
                                    ->GetBoundsInScreen()
                                    .CenterPoint());
    event_generator.ClickLeftButton();
  }

  EXPECT_EQ(app_id_pwa, pwa_app_info->launch_name);
  EXPECT_EQ(app_name_pwa, pwa_app_info->display_name);
  EXPECT_EQ(app_id_arc, arc_app_info->launch_name);
  EXPECT_EQ(app_name_arc, arc_app_info->display_name);

  // Check the status of the remember selection checkbox.
  ASSERT_TRUE(remember_selection_checkbox());
  EXPECT_TRUE(remember_selection_checkbox()->GetEnabled());
  EXPECT_FALSE(remember_selection_checkbox()->GetChecked());

  // Launch the app.
  intent_picker_bubble()->AcceptDialog();
  EXPECT_TRUE(VerifyPWALaunched(app_id_pwa));

  // WebApp histogram.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.WebApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened, 1);

  // ArcApp histogram.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2.ArcApp",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened, 0);

  // General histogram.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kEntryPointShown, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.LinkCapturingEvent2",
      apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened, 1);
}

// Test that bubble pops out when there is both PWA and ARC candidates, and
// test launch the ARC app.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       ARCAndPWACandidateLaunchARC) {
  GURL test_url(InScopeAppUrl());
  std::string app_name_pwa = "pwa_test_name";
  auto app_id_pwa = InstallWebApp(app_name_pwa, test_url);
  std::string app_name_arc = "arc_test_name";
  auto app_id_arc = AddArcAppWithIntentFilter(app_name_arc, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  EXPECT_EQ(2U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(2U, app_info.size());
  const apps::IntentPickerAppInfo* pwa_app_info;
  const apps::IntentPickerAppInfo* arc_app_info;
  if (app_info[0].launch_name == app_id_pwa) {
    pwa_app_info = &app_info[0];
    arc_app_info = &app_info[1];

    // Select the ARC app when it is not automatically selected.
    auto event_generator = ui::test::EventGenerator(
        views::GetRootWindow(intent_picker_bubble()->GetWidget()));

    event_generator.MoveMouseTo(GetButtonAtIndex(intent_picker_bubble(), 1)
                                    ->GetBoundsInScreen()
                                    .CenterPoint());
    event_generator.ClickLeftButton();
  } else {
    pwa_app_info = &app_info[1];
    arc_app_info = &app_info[0];
  }

  EXPECT_EQ(app_id_pwa, pwa_app_info->launch_name);
  EXPECT_EQ(app_name_pwa, pwa_app_info->display_name);
  EXPECT_EQ(app_id_arc, arc_app_info->launch_name);
  EXPECT_EQ(app_name_arc, arc_app_info->display_name);

  // Check the status of the remember selection checkbox.
  ASSERT_TRUE(remember_selection_checkbox());
  EXPECT_TRUE(remember_selection_checkbox()->GetEnabled());
  EXPECT_FALSE(remember_selection_checkbox()->GetChecked());

  // Launch the app.
  EXPECT_EQ(0U, launched_arc_apps().size());
  intent_picker_bubble()->AcceptDialog();
  ASSERT_NO_FATAL_FAILURE(VerifyArcAppLaunched(app_name_arc, test_url));
}

// Test that stay in chrome works when there is both PWA and ARC candidates.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       StayInChromeARCAndPWA) {
  GURL test_url(InScopeAppUrl());
  std::string app_name_pwa = "pwa_test_name";
  auto app_id_pwa = InstallWebApp(app_name_pwa, test_url);
  std::string app_name_arc = "arc_test_name";
  auto app_id_arc = AddArcAppWithIntentFilter(app_name_arc, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  ASSERT_NO_FATAL_FAILURE(CheckStayInChrome());
}

// Test that remember this choice checkbox works for open ARC app option.
//
// TODO(crbug.com/40863954): Fix timeouts under MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_RememberOpenARCApp DISABLED_RememberOpenARCApp
#else
#define MAYBE_RememberOpenARCApp RememberOpenARCApp
#endif
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestChromeOS,
                       MAYBE_RememberOpenARCApp) {
  GURL test_url(InScopeAppUrl());
  std::string app_name = "test_name";
  auto app_id = AddArcAppWithIntentFilter(app_name, test_url);

  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateAndWaitForIconUpdate(test_url);
  ClickIconToShowBubble();

  // Check "Remember my choice" and choose "Open App".
  ASSERT_TRUE(remember_selection_checkbox());
  ASSERT_TRUE(remember_selection_checkbox()->GetEnabled());
  remember_selection_checkbox()->SetChecked(true);
  ASSERT_TRUE(intent_picker_bubble());
  intent_picker_bubble()->AcceptDialog();
  WaitForAppService();

  // Navigate to the same site again, and verify the app is automatically
  // launched.
  clear_launched_arc_apps();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  NavigateParams params(browser(), test_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_NO_FATAL_FAILURE(VerifyArcAppLaunched(app_name, test_url));
}
