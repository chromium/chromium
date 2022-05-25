// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_no_creds_dialog_view.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"

namespace payments {

using ::testing::HasSubstr;

class SecurePaymentConfirmationNoCredsDialogViewTest
    : public DialogBrowserTest,
      public SecurePaymentConfirmationNoCredsDialogView::ObserverForTest {
 public:
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
    OPT_OUT_CLICKED,
  };

  // UiBrowserTest:
  // Note that ShowUi is only used for the InvokeUi_* tests.
  void ShowUi(const std::string& name) override {
    CreateAndShowDialog(u"merchant.example", false);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateAndShowDialog(const std::u16string& merchant_name,
                           bool showOptOut) {
    dialog_view_ =
        (new SecurePaymentConfirmationNoCredsDialogView(this))->GetWeakPtr();

    // The dialog decides whether to show the opt-out link or not based on
    // whether the callback is null or valid.
    base::OnceClosure opt_out_callback = base::NullCallback();
    if (showOptOut)
      opt_out_callback = base::DoNothing();
    dialog_view_->ShowDialog(GetActiveWebContents(), merchant_name, u"Opt Out",
                             base::DoNothing(), std::move(opt_out_callback));
  }

  const std::u16string& GetLabelText(
      SecurePaymentConfirmationNoCredsDialogView::DialogViewID view_id) {
    return static_cast<views::Label*>(
               dialog_view_->GetViewByID(static_cast<int>(view_id)))
        ->GetText();
  }

  void ResetEventWaiter(DialogEvent event) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
        std::list<DialogEvent>{event});
  }

  // SecurePaymentConfirmationNoCredsDialogView::ObserverForTest
  void OnDialogOpened() override {}
  void OnDialogClosed() override {}
  void OnOptOutClicked() override {
    event_waiter_->OnEvent(DialogEvent::OPT_OUT_CLICKED);
  }

 protected:
  base::WeakPtr<SecurePaymentConfirmationNoCredsDialogView> dialog_view_;

  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       Default) {
  CreateAndShowDialog(u"merchant.example", false);
  EXPECT_THAT(base::UTF16ToUTF8(
                  GetLabelText(SecurePaymentConfirmationNoCredsDialogView::
                                   DialogViewID::NO_MATCHING_CREDS_TEXT)),
              HasSubstr("merchant.example"));
  // The opt-out link should always be created, but should not be visible.
  ASSERT_TRUE(dialog_view_->GetExtraView());
  EXPECT_FALSE(dialog_view_->GetExtraView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest, OptOut) {
  CreateAndShowDialog(u"merchant.example", true);

  // The opt-out link should be created and visible.
  views::View* opt_out_view = dialog_view_->GetExtraView();
  EXPECT_NE(opt_out_view, nullptr);
  EXPECT_TRUE(opt_out_view->GetVisible());
  views::Link* opt_out_link = static_cast<views::Link*>(opt_out_view);
  EXPECT_EQ(u"Opt Out", opt_out_link->GetText());

  // Now click the Opt Out link and make sure that the expected events occur.
  ResetEventWaiter(DialogEvent::OPT_OUT_CLICKED);
  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  opt_out_link->OnMousePressed(pressed_event);
  opt_out_link->OnMouseReleased(pressed_event);

  // If we make it past this wait, the delegate was correctly called.
  event_waiter_->Wait();
}

}  // namespace payments
