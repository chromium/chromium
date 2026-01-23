// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

class FirstRunDialogTest : public DialogBrowserTest {
 public:
  FirstRunDialogTest() = default;
  FirstRunDialogTest(const FirstRunDialogTest&) = delete;
  FirstRunDialogTest& operator=(const FirstRunDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto on_close_callback = base::BindOnce(
        [](bool* out_result, bool closed_through_accept_button) {
          *out_result = closed_through_accept_button;
        },
        &closed_through_accept_button_);

    FirstRunDialog::Show(base::DoNothing(), std::move(on_close_callback));
  }

 protected:
  // Needs to be set by each test to verify the callback result. This would
  // ideally be optional, but because we can't capture `this` and because the
  // callback requires a `bool`, we're stuck with this approach.
  bool closed_through_accept_button_;
};

IN_PROC_BROWSER_TEST_F(FirstRunDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FirstRunDialogTest, CallbackTrueIfAccepted) {
  // Assign to false to ensure this test will fail if the callback doesn't
  // properly set it to true.
  closed_through_accept_button_ = false;

  views::NamedWidgetShownWaiter dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "FirstRunDialog");
  ShowUi("CallbackTrueIfAccepted");
  views::Widget* dialog_widget = dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(dialog_widget, nullptr);

  {
    // Uncheck the "Make default" checkbox to prevent attempting to show the OS
    // default browser UI.
    FirstRunDialog::TestApi test_api(
        static_cast<FirstRunDialog*>(dialog_widget->widget_delegate()));
    test_api.SetMakeDefaultCheckboxChecked(false);
  }

  views::test::AcceptDialog(dialog_widget);
  EXPECT_TRUE(closed_through_accept_button_);
}

IN_PROC_BROWSER_TEST_F(FirstRunDialogTest, CallbackFalseIfNotAccepted) {
  // Assign to true to ensure this test will fail if the callback doesn't
  // properly set it to false.
  closed_through_accept_button_ = true;

  views::NamedWidgetShownWaiter dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "FirstRunDialog");
  ShowUi("CallbackFalseIfNotAccepted");
  views::Widget* dialog_widget = dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(dialog_widget, nullptr);

  views::test::CancelDialog(dialog_widget);
  EXPECT_FALSE(closed_through_accept_button_);
}
