// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace omnibox_everywhere {

class OmniboxEverywhereBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxEverywhereBrowserTest() = default;
  ~OmniboxEverywhereBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndCloseWidget) {
  OmniboxEverywhereUIManager ui_manager;

  EXPECT_FALSE(ui_manager.widget_for_testing());

  // Show the widget.
  ui_manager.Show();

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

}  // namespace omnibox_everywhere
