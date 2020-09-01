// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

class ExtensionsToolbarInteractiveUiTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionsToolbarInteractiveUiTest() = default;

  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_container();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
  }

 private:
  ui::ScopedAnimationDurationScaleMode disable_animation_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// Tests that clicking an extension toolbar icon when the popup is open closes
// the popup.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarInteractiveUiTest,
                       DoubleClickToolbarActionToClose) {
  const extensions::Extension* const extension =
      LoadExtension(test_data_dir_.AppendASCII("ui/browser_action_popup"));
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);
  EXPECT_TRUE(toolbar_model->IsActionPinned(extension->id()));

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  views::test::WaitForAnimatingLayoutManager(container);

  ToolbarActionViewController* const view_controller =
      container->GetActionForId(extension->id());
  EXPECT_TRUE(container->IsActionVisibleOnToolbar(view_controller));
  ToolbarActionView* const action_view =
      container->GetViewForId(extension->id());
  EXPECT_TRUE(action_view->GetVisible());

  ExtensionTestMessageListener listener("Popup opened", /*will_reply=*/false);
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
      ui_test_utils::GetCenterInScreenCoordinates(action_view)));
  EXPECT_TRUE(ui_controls::SendMouseClick(ui_controls::LEFT));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(view_controller->IsShowingPopup());
  EXPECT_EQ(view_controller, container->popup_owner_for_testing());

  // TODO(devlin): Update this use an ExtensionHostObserver, rather than the
  // deprecated notifications system.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));
  observer.Wait();

  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, container->popup_owner_for_testing());

  // Releasing the mouse shouldn't result in the popup being shown again.
  EXPECT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(view_controller->IsShowingPopup());
  EXPECT_EQ(nullptr, container->popup_owner_for_testing());
}
