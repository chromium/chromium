// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/storage/storage_pressure_bubble_view.h"

#include "base/test/metrics/histogram_tester.h"
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
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectBucketCount("Storage.StoragePressure.Bubble",
                                     /*kOpenedAllSites=*/2, 1);
  histogram_tester.ExpectBucketCount("Storage.StoragePressure.Bubble",
                                     /*kIgnored=*/1, 0);
}

IN_PROC_BROWSER_TEST_F(StoragePressureBubbleViewTest, CancelDialog) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectBucketCount("Storage.StoragePressure.Bubble",
                                     /*kOpenedAllSites=*/2, 0);
  histogram_tester.ExpectBucketCount("Storage.StoragePressure.Bubble",
                                     /*kIgnored=*/1, 1);
}
