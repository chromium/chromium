// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace omnibox_everywhere {

class OmniboxEverywhereBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxEverywhereBrowserTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }
  ~OmniboxEverywhereBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndCloseWidget) {
  OmniboxEverywhereUIManager ui_manager;

  EXPECT_FALSE(ui_manager.widget_for_testing());

  // Show the widget.
  ui_manager.ShowForProfile(browser()->profile(),
                            browser()->GetWindow()->GetNativeWindow());

  views::Widget* widget = ui_manager.widget_for_testing();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Check the widget delegate and its properties.
  views::WidgetDelegate* delegate = widget->widget_delegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->CanActivate());
  EXPECT_FALSE(delegate->CanMaximize());
  EXPECT_FALSE(delegate->CanMinimize());
  EXPECT_FALSE(delegate->CanResize());

  // Close the widget and wait for destruction.
  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager.Close();
  waiter.Wait();

  EXPECT_FALSE(ui_manager.widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, FocusAndActivationState) {
  OmniboxEverywhereUIManager ui_manager;

  ui_manager.ShowForProfile(browser()->profile(),
                            browser()->GetWindow()->GetNativeWindow());
  views::Widget* widget = ui_manager.widget_for_testing();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(widget->IsActive());

  views::test::WidgetDestroyedWaiter waiter(widget);
  ui_manager.Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndDismissViaHotkey) {
  OmniboxEverywhereController* controller =
      g_browser_process->GetFeatures()->omnibox_everywhere_controller();
  ASSERT_TRUE(controller);

  EXPECT_FALSE(controller->IsVisible());

  const ui::Accelerator default_hotkey(
      ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);

  // Trigger hotkey to show widget.
  controller->OnKeyPressed(default_hotkey);
  EXPECT_TRUE(controller->IsVisible());

  views::Widget* widget = controller->ui_manager()->widget_for_testing();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Trigger hotkey again to dismiss widget.
  views::test::WidgetDestroyedWaiter waiter(widget);
  controller->OnKeyPressed(default_hotkey);
  waiter.Wait();

  EXPECT_FALSE(controller->IsVisible());
  EXPECT_FALSE(controller->ui_manager()->widget_for_testing());
}

}  // namespace omnibox_everywhere
