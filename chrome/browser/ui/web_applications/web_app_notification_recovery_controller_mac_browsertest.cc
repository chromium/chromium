// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_notification_recovery_controller_mac.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace web_app {
namespace {

enum class WebAppWindowMode { kBrowserTab, kAppWindow };

class WebAppNotificationRecoveryControllerBrowserTestBase
    : public WebAppBrowserTestBase {
 public:
  WebAppNotificationRecoveryControllerBrowserTestBase()
      : WebAppBrowserTestBase({features::kAppShimNotificationAttribution}, {}) {
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    WebAppToolbarButtonContainer::DisableAnimationForTesting(true);
    start_url_ = embedded_https_test_server().GetURL("/web_apps/basic.html");
    out_of_scope_url_ = embedded_https_test_server().GetURL("/title1.html");
    app_id_ = InstallPWA(start_url_);
  }

  void TearDownOnMainThread() override {
    paint_as_active_lock_.reset();
    target_browser_ = nullptr;
    UninstallWebApp(app_id_);
    WebAppToolbarButtonContainer::DisableAnimationForTesting(false);
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  void SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus status) {
    AppShimHost* app_shim_host =
        apps::AppShimManager::Get()->FindOrCreateHostForTesting(profile(),
                                                                app_id_);
    ASSERT_TRUE(app_shim_host);
    app_shim_host->SetNotificationPermissionStatusForTesting(status);
  }

  void AllowNotificationsInChrome() {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(start_url_, start_url_,
                                        ContentSettingsType::NOTIFICATIONS,
                                        CONTENT_SETTING_ALLOW);
  }

 protected:
  GURL start_url_;
  GURL out_of_scope_url_;
  webapps::AppId app_id_;
  raw_ptr<BrowserWindowInterface> target_browser_ = nullptr;
  std::unique_ptr<views::Widget::PaintAsActiveLock> paint_as_active_lock_;
};

class WebAppNotificationRecoveryControllerBrowserTest
    : public SupportsTestDialog<
          WebAppNotificationRecoveryControllerBrowserTestBase>,
      public testing::WithParamInterface<WebAppWindowMode> {
 public:
  // TestBrowserDialog:
  void ShowUi(const std::string& name) override {
    // Bounds can exceed the display work area on bots when the bubble is
    // anchored to the top-right PWA titlebar.
    set_should_verify_dialog_bounds(false);
    AllowNotificationsInChrome();
    if (name == "NotificationSystemPermissionDenied") {
      SetSystemNotificationPermissionStatus(
          mac_notifications::mojom::PermissionStatus::kDenied);
    } else {
      ASSERT_EQ(name, "NotificationSystemPermissionNotDetermined");
      SetSystemNotificationPermissionStatus(
          mac_notifications::mojom::PermissionStatus::kNotDetermined);
    }
    OpenTargetWebApp();

    ContentSettingImageView* view = GetNotificationImageView();
    ASSERT_TRUE(view);
    EXPECT_TRUE(view->GetVisible());
    EXPECT_TRUE(view->ShouldShowLabel());
    EXPECT_TRUE(view->IsBubbleShowing());
  }

  void OpenTargetWebApp() {
    switch (GetParam()) {
      case WebAppWindowMode::kBrowserTab:
        target_browser_ = browser();
        paint_as_active_lock_ = BrowserView::GetBrowserViewForBrowser(browser())
                                    ->GetWidget()
                                    ->LockPaintAsActive();
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url_));
        break;
      case WebAppWindowMode::kAppWindow: {
        target_browser_ = LaunchWebAppBrowserAndWait(app_id_);
        ASSERT_TRUE(target_browser_);
        views::Widget* app_widget =
            BrowserView::GetBrowserViewForBrowser(target_browser_)->GetWidget();
        paint_as_active_lock_ = app_widget->LockPaintAsActive();
        views::test::WidgetVisibleWaiter(app_widget).Wait();
        break;
      }
    }
  }

  void NavigateInTargetBrowser(const GURL& url) {
    if (!target_browser_) {
      ASSERT_EQ(url, start_url_);
      OpenTargetWebApp();
      return;
    }
    ASSERT_TRUE(ui_test_utils::NavigateToURL(target_browser_, url));
  }

  ContentSettingImageView* GetNotificationImageView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(target_browser_);
    const auto& views = GetParam() == WebAppWindowMode::kAppWindow
                            ? browser_view->web_app_frame_toolbar_for_testing()
                                  ->get_right_container_for_testing()
                                  ->content_settings_container()
                                  ->get_content_setting_views()
                            : browser_view->GetLocationBarView()
                                  ->GetContentSettingViewsForTest();
    auto it = std::ranges::find(
        views, ContentSettingImageModel::ImageType::kNotifications,
        &ContentSettingImageView::GetType);
    return it != views.end() ? *it : nullptr;
  }
};

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       InvokeUi_NotificationSystemPermissionNotDetermined) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       PrepromptBubbleAllowRequestsOsPermission) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kNotDetermined);

  base::HistogramTester histogram_tester;
  bool os_permission_requested = false;
  apps::AppShimManager::Get()
      ->SetNotificationPermissionRequestHandlerForTesting(
          base::BindLambdaForTesting(
              [&](const webapps::AppId& app_id,
                  apps::AppShimManager::RequestNotificationPermissionCallback
                      callback) {
                EXPECT_EQ(app_id_, app_id);
                os_permission_requested = true;
                SetSystemNotificationPermissionStatus(
                    mac_notifications::mojom::PermissionStatus::kPromptPending);
                std::move(callback).Run(
                    mac_notifications::mojom::RequestPermissionResult::
                        kPermissionGranted);
              }));
  base::ScopedClosureRunner reset_handler(base::BindOnce([]() {
    apps::AppShimManager::Get()
        ->SetNotificationPermissionRequestHandlerForTesting({});
  }));

  OpenTargetWebApp();
  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
  ASSERT_TRUE(view->IsBubbleShowing());
  ASSERT_TRUE(view->GetBubbleViewForTesting());

  views::test::WidgetDestroyedWaiter bubble_destroyed_waiter(
      view->GetBubbleWidget());
  view->GetBubbleViewForTesting()->AcceptDialog();
  bubble_destroyed_waiter.Wait();
  view->slide_animation_for_testing().End();

  EXPECT_TRUE(os_permission_requested);
  histogram_tester.ExpectUniqueSample(
      "Notifications.Permissions.AppShim.OSPermissionRequestResult."
      "FromNotificationsBubble",
      mac_notifications::mojom::RequestPermissionResult::kPermissionGranted, 1);
  EXPECT_FALSE(view->IsBubbleShowing());
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());

  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kGranted);
  EXPECT_FALSE(view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       PrepromptBubbleDontAllowBlocksInChromeAndHidesIcon) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kNotDetermined);
  OpenTargetWebApp();
  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->GetVisible());
  ASSERT_TRUE(view->IsBubbleShowing());
  ASSERT_TRUE(view->GetBubbleViewForTesting());

  views::test::WidgetDestroyedWaiter bubble_destroyed_waiter(
      view->GetBubbleWidget());
  view->GetBubbleViewForTesting()->CancelDialog();
  bubble_destroyed_waiter.Wait();

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            HostContentSettingsMapFactory::GetForProfile(profile())
                ->GetContentSetting(start_url_, start_url_,
                                    ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(view->IsBubbleShowing());
  EXPECT_FALSE(view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       InvokeUi_NotificationSystemPermissionDenied) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       AutoOpensOnNavigationForPromptPending) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kPromptPending);
  OpenTargetWebApp();

  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_TRUE(view->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       DoesNotAutoOpenOnNavigationForExplicitlyDenied) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kGranted);
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kDenied);
  OpenTargetWebApp();

  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  // Explicit OS denial should not proactively surface an icon or bubble on
  // navigation before the page interacts with notifications.
  EXPECT_FALSE(view->GetVisible());
  EXPECT_FALSE(view->IsBubbleShowing());

  content::WebContents* web_contents =
      target_browser_->GetTabStripModel()->GetActiveWebContents();
  content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents->GetPrimaryMainFrame())
      ->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
  EXPECT_FALSE(view->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       MidPageGrantAutoOpensOnWindowReactivationOnce) {
  OpenTargetWebApp();
  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->GetVisible());
  EXPECT_FALSE(view->IsBubbleShowing());

  // Exercise the real `Notification.requestPermission()` -> prompt accept ->
  // prompt widget close -> `ShowNotificationPermissionRequest()` flow while
  // macOS transitions to `kPromptPending`: closing the Chrome permission prompt
  // bubble must not prematurely auto-open the recovery bubble.
  bool os_permission_requested = false;
  apps::AppShimManager::RequestNotificationPermissionCallback
      pending_os_permission_callback;
  apps::AppShimManager::Get()
      ->SetNotificationPermissionRequestHandlerForTesting(
          base::BindLambdaForTesting(
              [&](const webapps::AppId& app_id,
                  apps::AppShimManager::RequestNotificationPermissionCallback
                      callback) {
                EXPECT_EQ(app_id_, app_id);
                os_permission_requested = true;
                pending_os_permission_callback = std::move(callback);
                SetSystemNotificationPermissionStatus(
                    mac_notifications::mojom::PermissionStatus::kPromptPending);
              }));
  base::ScopedClosureRunner reset_handler(base::BindOnce([]() {
    apps::AppShimManager::Get()
        ->SetNotificationPermissionRequestHandlerForTesting({});
  }));

  content::WebContents* web_contents =
      target_browser_->GetTabStripModel()->GetActiveWebContents();
  permissions::PermissionRequestObserver prompt_observer(web_contents);
  content::ExecuteScriptAsync(web_contents, "Notification.requestPermission()");
  prompt_observer.Wait();

  test::PermissionRequestManagerTestApi prompt_test_api(target_browser_);
  if (!prompt_test_api.GetPromptWindow()) {
    // In `kBrowserTab` mode, notification permission requests use a left-side
    // chip (`LOCATION_BAR_LEFT_CHIP`) whose bubble opens when clicked. Click
    // the chip so the real child prompt bubble widget is open when accepted.
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(target_browser_)
            ->GetLocationBarView();
    ASSERT_TRUE(location_bar_view);
    ASSERT_TRUE(location_bar_view->GetChipController());
    location_bar_view->GetChipController()->chip()->ExecuteForTesting();
  }
  views::Widget* prompt_widget = prompt_test_api.GetPromptWindow();
  ASSERT_TRUE(prompt_widget);
  views::Widget* target_widget =
      BrowserView::GetBrowserViewForBrowser(target_browser_)->GetWidget();

  // Transfer active state to `prompt_widget` and release the test's own lock so
  // `target_widget->ShouldPaintAsActive()` stays true solely via
  // `prompt_widget`'s `parent_paint_as_active_lock_`, matching production.
  prompt_widget->OnNativeWidgetActivationChanged(true);
  target_widget->OnNativeWidgetActivationChanged(false);
  paint_as_active_lock_.reset();
  ASSERT_TRUE(target_widget->ShouldPaintAsActive());

  views::test::WidgetDestroyedWaiter prompt_destroyed_waiter(prompt_widget);
  prompt_test_api.manager()->Accept(/*prompt_options=*/std::monostate());
  // `Accept()` marks `prompt_widget` closed; deactivating `prompt_widget` while
  // closed must retain `parent_paint_as_active_lock_` until `target_widget`
  // regains focus and `prompt_widget` finishes destroying, avoiding a
  // `true -> false -> true` bounce on `target_widget->ShouldPaintAsActive()`.
  prompt_widget->OnNativeWidgetActivationChanged(false);
  EXPECT_TRUE(target_widget->ShouldPaintAsActive());
  EXPECT_FALSE(view->IsBubbleShowing());

  paint_as_active_lock_ = target_widget->LockPaintAsActive();
  prompt_destroyed_waiter.Wait();
  EXPECT_TRUE(os_permission_requested);

  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_FALSE(view->IsBubbleShowing());

  // Simulate switching away (e.g. to the App Shim prompt / another app) and
  // returning to the browser window without granting OS permission: the
  // recovery bubble now auto-opens.
  target_widget->OnNativeWidgetActivationChanged(false);
  paint_as_active_lock_.reset();
  ASSERT_FALSE(target_widget->ShouldPaintAsActive());
  EXPECT_FALSE(view->IsBubbleShowing());

  paint_as_active_lock_ = target_widget->LockPaintAsActive();
  EXPECT_TRUE(target_widget->ShouldPaintAsActive());
  EXPECT_TRUE(view->IsBubbleShowing());

  // Dismissing the bubble collapses the pill; cycling window activation again
  // on the same page should not auto-open the bubble a second time.
  ASSERT_TRUE(view->GetBubbleWidget());
  view->GetBubbleWidget()->CloseNow();
  view->slide_animation_for_testing().End();
  EXPECT_FALSE(view->IsBubbleShowing());
  EXPECT_FALSE(view->ShouldShowLabel());

  target_widget->OnNativeWidgetActivationChanged(false);
  paint_as_active_lock_.reset();
  ASSERT_FALSE(target_widget->ShouldPaintAsActive());
  paint_as_active_lock_ = target_widget->LockPaintAsActive();
  EXPECT_TRUE(target_widget->ShouldPaintAsActive());
  EXPECT_FALSE(view->IsBubbleShowing());
  EXPECT_FALSE(view->ShouldShowLabel());

  // Navigating to another in-scope page in the same tab should keep the
  // indicator continuously visible (without re-expanding the already-collapsed
  // label or re-opening the already-dismissed bubble).
  const GURL next_url =
      embedded_https_test_server().GetURL("/web_apps/basic.html?next");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(target_browser_, next_url));
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
  EXPECT_FALSE(view->IsBubbleShowing());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebAppNotificationRecoveryControllerBrowserTest,
    testing::Values(WebAppWindowMode::kBrowserTab,
                    WebAppWindowMode::kAppWindow),
    [](const testing::TestParamInfo<WebAppWindowMode>& info) {
      switch (info.param) {
        case WebAppWindowMode::kBrowserTab:
          return "BrowserTab";
        case WebAppWindowMode::kAppWindow:
          return "AppWindow";
      }
    });

IN_PROC_BROWSER_TEST_F(WebAppNotificationRecoveryControllerBrowserTestBase,
                       BackgroundTabAutoOpensOnTabActivationOnce) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->GetLocationBarView());

  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kDenied);
  paint_as_active_lock_ = browser_view->GetWidget()->LockPaintAsActive();

  // Open the web app in a background tab while about:blank remains active.
  content::TestNavigationObserver nav_observer(start_url_);
  nav_observer.StartWatchingNewWebContents();
  chrome::AddTabAt(browser(), start_url_, /*index=*/1, /*foreground=*/false);
  nav_observer.Wait();
  ASSERT_EQ(0, browser()->GetTabStripModel()->active_index());

  const auto& views =
      browser_view->GetLocationBarView()->GetContentSettingViewsForTest();
  auto it = std::ranges::find(
      views, ContentSettingImageModel::ImageType::kNotifications,
      &ContentSettingImageView::GetType);
  ASSERT_NE(it, views.end());
  ContentSettingImageView* view = *it;
  EXPECT_FALSE(view->GetVisible());
  EXPECT_FALSE(view->IsBubbleShowing());

  // Activating the web app tab should surface the pill and auto-open the
  // recovery bubble.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_TRUE(view->IsBubbleShowing());

  // Dismissing the bubble and switching away and back should not auto-open on
  // the same page again.
  ASSERT_TRUE(view->GetBubbleWidget());
  view->GetBubbleWidget()->CloseNow();
  EXPECT_FALSE(view->IsBubbleShowing());

  browser()->GetTabStripModel()->ActivateTabAt(0);
  EXPECT_FALSE(view->GetVisible());
  browser()->GetTabStripModel()->ActivateTabAt(1);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(WebAppNotificationRecoveryControllerBrowserTestBase,
                       AppWindowAutoOpensWhenKeyArrivesBeforeVisibility) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kDenied);

  // Simulate out-of-process RemoteCocoa App Shim launch: the browser-side
  // window and initial navigation commit before the App Shim connects and
  // executes `-[NSWindow makeKeyAndOrderFront:]`.
  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
          GenerateApplicationNameFromAppId(app_id_),
          /*trusted_source=*/true, gfx::Rect(), profile(),
          /*user_gesture=*/true));
  ASSERT_TRUE(app_browser);
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  views::Widget* app_widget = app_browser_view->GetWidget();
  ASSERT_FALSE(app_widget->IsVisible());
  ASSERT_FALSE(app_widget->ShouldPaintAsActive());

  content::TestNavigationObserver nav_observer(start_url_);
  nav_observer.StartWatchingNewWebContents();
  chrome::AddTabAt(app_browser, start_url_, /*index=*/0, /*foreground=*/true,
                   /*group=*/std::nullopt, /*pinned=*/false,
                   NavigateParams::WindowAction::kNoAction);
  nav_observer.Wait();

  const auto& views = app_browser_view->web_app_frame_toolbar_for_testing()
                          ->get_right_container_for_testing()
                          ->content_settings_container()
                          ->get_content_setting_views();
  auto it = std::ranges::find(
      views, ContentSettingImageModel::ImageType::kNotifications,
      &ContentSettingImageView::GetType);
  ASSERT_NE(it, views.end());
  ContentSettingImageView* view = *it;

  // Before the window is shown, the titlebar chip is already expanded from the
  // committed navigation, while the bubble waits for visibility + activation.
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_FALSE(view->IsBubbleShowing());

  // AppKit's `-[NSWindow makeKeyAndOrderFront:]` invokes
  // `_makeKeyRegardlessOfVisibility` before `orderWindow:relativeTo:`, so
  // `ShouldPaintAsActive()` becomes true while `IsVisible()` is still false.
  paint_as_active_lock_ = app_widget->LockPaintAsActive();
  ASSERT_TRUE(app_widget->ShouldPaintAsActive());
  ASSERT_FALSE(app_widget->IsVisible());
  EXPECT_FALSE(view->IsBubbleShowing());

  // When `OnVisibilityChanged(true)` subsequently arrives (`Show()`),
  // `OnWidgetVisibilityChanged` must trigger the recovery bubble immediately
  // without waiting for a future window deactivation/reactivation cycle.
  app_browser->GetWindow()->Show();
  views::test::WidgetVisibleWaiter(app_widget).Wait();
  EXPECT_TRUE(app_widget->IsVisible());
  EXPECT_TRUE(view->IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_P(WebAppNotificationRecoveryControllerBrowserTest,
                       FrequencyLimitForAutoOpenBubble) {
  AllowNotificationsInChrome();
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kDenied);

  const int max_auto_open_count =
      features::kAppShimNotificationAttributionMaxAutoOpenBubbleCount.Get();

  // First `max_auto_open_count` occurrences: the recovery bubble auto-opens.
  for (int i = 1; i <= max_auto_open_count; ++i) {
    NavigateInTargetBrowser(start_url_);
    ContentSettingImageView* view = GetNotificationImageView();
    ASSERT_TRUE(view);
    EXPECT_TRUE(view->GetVisible());
    ASSERT_TRUE(view->IsBubbleShowing());
    view->slide_animation_for_testing().End();
    EXPECT_TRUE(view->ShouldShowLabel());
    EXPECT_EQ(i,
              AppShimRegistry::Get()->GetNotificationBubbleAutoOpenCountForApp(
                  app_id_));

    ASSERT_TRUE(view->GetBubbleWidget());
    views::test::WidgetDestroyedWaiter bubble_destroyed_waiter(
        view->GetBubbleWidget());
    NavigateInTargetBrowser(out_of_scope_url_);
    bubble_destroyed_waiter.Wait();
    EXPECT_FALSE(view->GetVisible());
    EXPECT_FALSE(view->IsBubbleShowing());
  }

  // Subsequent occurrences: the bubble no longer auto-opens, while the
  // "Notifications off" label still expands.
  NavigateInTargetBrowser(start_url_);
  ContentSettingImageView* view = GetNotificationImageView();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->IsBubbleShowing());
  EXPECT_TRUE(view->is_animating_label());
  EXPECT_TRUE(view->ShouldShowLabel());
  view->slide_animation_for_testing().End();
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_EQ(max_auto_open_count,
            AppShimRegistry::Get()->GetNotificationBubbleAutoOpenCountForApp(
                app_id_));

  // Granting OS notification permission resets the counter.
  NavigateInTargetBrowser(out_of_scope_url_);
  EXPECT_FALSE(view->GetVisible());
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kGranted);
  EXPECT_EQ(0, AppShimRegistry::Get()->GetNotificationBubbleAutoOpenCountForApp(
                   app_id_));

  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kNotDetermined);
  SetSystemNotificationPermissionStatus(
      mac_notifications::mojom::PermissionStatus::kDenied);
  NavigateInTargetBrowser(start_url_);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->IsBubbleShowing());
  view->slide_animation_for_testing().End();
  EXPECT_TRUE(view->ShouldShowLabel());
  EXPECT_EQ(1, AppShimRegistry::Get()->GetNotificationBubbleAutoOpenCountForApp(
                   app_id_));
}

}  // namespace
}  // namespace web_app
