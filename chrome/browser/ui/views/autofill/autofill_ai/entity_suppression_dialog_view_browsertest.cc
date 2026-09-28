// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/entity_suppression_dialog_view.h"

#include <string>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

class EntitySuppressionDialogViewBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowEntitySuppressionDialogView(
        browser()->GetTabStripModel()->GetActiveWebContents(),
        /*num_sources=*/1, callback_.Get());
  }

 protected:
  // Shows the dialog and returns its widget.
  views::Widget* ShowDialog() {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kEntitySuppressionDialogName);
    ShowUi("default");
    return waiter.WaitIfNeededAndGet();
  }

  base::MockCallback<base::OnceCallback<void(bool)>> callback_;
};

IN_PROC_BROWSER_TEST_F(EntitySuppressionDialogViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that accepting the dialog runs the callback once, with true.
IN_PROC_BROWSER_TEST_F(EntitySuppressionDialogViewBrowserTest, Accept) {
  views::Widget* widget = ShowDialog();
  ASSERT_TRUE(widget);

  EXPECT_CALL(callback_, Run(true));
  views::test::AcceptDialog(widget);
}

// Tests that cancelling the dialog runs the callback once, with false.
IN_PROC_BROWSER_TEST_F(EntitySuppressionDialogViewBrowserTest, Cancel) {
  views::Widget* widget = ShowDialog();
  ASSERT_TRUE(widget);

  EXPECT_CALL(callback_, Run(false));
  views::test::CancelDialog(widget);
}

// Tests that closing the dialog without accepting or cancelling it runs the
// callback once, with false.
IN_PROC_BROWSER_TEST_F(EntitySuppressionDialogViewBrowserTest, Close) {
  views::Widget* widget = ShowDialog();
  ASSERT_TRUE(widget);

  EXPECT_CALL(callback_, Run(false));
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  waiter.Wait();
}

}  // namespace
}  // namespace autofill
