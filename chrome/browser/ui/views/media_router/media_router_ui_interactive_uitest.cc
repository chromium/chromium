// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/app_menu_test_api.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace media_router {

class MediaRouterUIInteractiveUITest : public InProcessBrowserTest {
 public:
  MediaRouterUIInteractiveUITest() = default;
  ~MediaRouterUIInteractiveUITest() override = default;

  // Returns the dialog controller for the active WebContents.
  MediaRouterDialogController* GetDialogController() {
    return MediaRouterDialogController::GetOrCreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  views::Widget* GetDialogWidget() {
    // interactive_ui_tests are not run on android, so
    // MediaRouterDialogControllerViews is the only implementation of
    // MediaRouterDialogController.
    return static_cast<MediaRouterDialogControllerViews*>(GetDialogController())
        ->GetCastDialogCoordinatorForTesting()
        .GetCastDialogWidget();
  }

  ui::SimpleMenuModel* GetIconContextMenu() {
    return static_cast<ui::SimpleMenuModel*>(GetCastIcon()->menu_model());
  }

  void WaitForAnimations() {
    auto* container = BrowserView::GetBrowserViewForBrowser(browser())
                          ->toolbar()
                          ->pinned_toolbar_actions_container();
    views::test::WaitForAnimatingLayoutManager(container);
  }

  void PressToolbarIcon() {
    WaitForAnimations();
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        GetCastIcon(), ui::test::InteractionTestUtil::InputType::kMouse);
  }

  bool ToolbarIconExists() {
    base::RunLoop().RunUntilIdle();
    ToolbarButton* cast_icon = GetCastIcon();
    return cast_icon && cast_icon->GetVisible();
  }

  void SetAlwaysShowActionPref(bool always_show) {
    CastToolbarButtonController::SetAlwaysShowActionPref(browser()->profile(),
                                                         always_show);
  }

 private:
  ToolbarButton* GetCastIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->GetCastButton();
  }
};

IN_PROC_BROWSER_TEST_F(MediaRouterUIInteractiveUITest,
                       OpenDialogFromContextMenu) {
  // Start with one tab showing about:blank.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogController* dialog_controller = GetDialogController();
  content::ContextMenuParams params;
  params.page_url =
      web_contents->GetController().GetLastCommittedEntry()->GetURL();
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_ROUTE_MEDIA));
  ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  menu.ExecuteCommand(IDC_ROUTE_MEDIA, 0);
  views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
  dialog_controller->HideMediaRouterDialog();
  waiter.Wait();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIInteractiveUITest, OpenDialogFromAppMenu) {
  // Start with one tab showing about:blank.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  std::unique_ptr<test::AppMenuTestApi> app_menu_test_api =
      test::AppMenuTestApi::Create(browser());
  app_menu_test_api->ShowMenu();

  MediaRouterDialogController* dialog_controller = GetDialogController();
  ASSERT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  app_menu_test_api->ExecuteCommand(IDC_ROUTE_MEDIA);
  views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
  dialog_controller->HideMediaRouterDialog();
  waiter.Wait();
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIInteractiveUITest,
                       EphemeralToolbarIconForDialog) {
  MediaRouterDialogController* dialog_controller = GetDialogController();

  {
    EXPECT_FALSE(ToolbarIconExists());
    dialog_controller->ShowMediaRouterDialog(
        MediaRouterDialogActivationLocation::PAGE);
    views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
    EXPECT_TRUE(ToolbarIconExists());
  }

  {
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    dialog_controller->HideMediaRouterDialog();
    waiter.Wait();
    EXPECT_FALSE(ToolbarIconExists());
  }

  {
    dialog_controller->ShowMediaRouterDialog(
        MediaRouterDialogActivationLocation::PAGE);
    WaitForAnimations();
    views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
    EXPECT_TRUE(ToolbarIconExists());
  }

  {
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    // Clicking on the toolbar icon should hide both the dialog and the icon.
    ASSERT_TRUE(ToolbarIconExists());
    PressToolbarIcon();
    waiter.Wait();
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
    EXPECT_FALSE(ToolbarIconExists());
  }

  {
    dialog_controller->ShowMediaRouterDialog(
        MediaRouterDialogActivationLocation::PAGE);
    WaitForAnimations();
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    SetAlwaysShowActionPref(true);
    // When the pref is set to true, hiding the dialog shouldn't hide the icon.
    dialog_controller->HideMediaRouterDialog();
    waiter.Wait();
    EXPECT_TRUE(ToolbarIconExists());
  }

  {
    dialog_controller->ShowMediaRouterDialog(
        MediaRouterDialogActivationLocation::PAGE);
    // While the dialog is showing, setting the pref to false shouldn't hide the
    // icon.
    WaitForAnimations();
    SetAlwaysShowActionPref(false);
    views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
    EXPECT_TRUE(ToolbarIconExists());
  }

  {
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    dialog_controller->HideMediaRouterDialog();
    waiter.Wait();
    EXPECT_FALSE(ToolbarIconExists());
  }
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIInteractiveUITest, PinAndUnpinToolbarIcon) {
  if (features::IsToolbarPinningEnabled()) {
    GTEST_SKIP() << "This test is not relevant for toolbar pinning as pinning "
                    "will now be handled by PinnedActionToolbarButton.";
  }

  GetDialogController()->ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation::PAGE);
  views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
  EXPECT_TRUE(ToolbarIconExists());
  // Pin the icon via its context menu.
  ui::SimpleMenuModel* context_menu = GetIconContextMenu();
  const size_t command_index =
      context_menu
          ->GetIndexOfCommandId(IDC_MEDIA_ROUTER_ALWAYS_SHOW_TOOLBAR_ACTION)
          .value();
  context_menu->ActivatedAt(command_index);

  views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
  GetDialogController()->HideMediaRouterDialog();
  waiter.Wait();
  EXPECT_TRUE(ToolbarIconExists());

  // Unpin the icon via its context menu.
  GetIconContextMenu()->ActivatedAt(command_index);
  EXPECT_FALSE(ToolbarIconExists());
}

IN_PROC_BROWSER_TEST_F(MediaRouterUIInteractiveUITest,
                       OpenDialogWithMediaRouterAction) {
  MediaRouterDialogController* dialog_controller = GetDialogController();
  // We start off at about:blank page.
  // Make sure there is 1 tab and media router is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(ToolbarIconExists());

  PressToolbarIcon();
  views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Reload the browser and wait.
  {
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    content::TestNavigationObserver reload_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    reload_observer.Wait();

    waiter.Wait();
    // The reload should have closed the dialog.
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  }

  PressToolbarIcon();
  views::test::WidgetVisibleWaiter(GetDialogWidget()).Wait();
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  {
    views::test::WidgetDestroyedWaiter waiter(GetDialogWidget());
    // Navigate away.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    waiter.Wait();
    // The navigation should have closed the dialog.
    EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());
  }
  SetAlwaysShowActionPref(false);
}

}  // namespace media_router
