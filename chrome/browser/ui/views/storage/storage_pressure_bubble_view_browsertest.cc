// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/storage/storage_pressure_bubble_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/storage_pressure_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

class StoragePressureBubbleViewTest : public DialogBrowserTest {
 public:
  StoragePressureBubbleViewTest() = default;
  ~StoragePressureBubbleViewTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowStoragePressureBubble(url::Origin::Create(GURL("https://example.com")));
  }
};

IN_PROC_BROWSER_TEST_F(StoragePressureBubbleViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(StoragePressureBubbleViewTest, AcceptDialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "StoragePressureBubbleView");

  ShowUi("default");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  int initial_tab_count = browser()->tab_strip_model()->count();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();

  EXPECT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            GURL("chrome://settings/content/all?sort=data-stored"));
}

IN_PROC_BROWSER_TEST_F(StoragePressureBubbleViewTest, CancelDialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "StoragePressureBubbleView");

  ShowUi("default");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  int initial_tab_count = browser()->tab_strip_model()->count();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  destroy_waiter.Wait();

  EXPECT_EQ(browser()->tab_strip_model()->count(), initial_tab_count);
}
