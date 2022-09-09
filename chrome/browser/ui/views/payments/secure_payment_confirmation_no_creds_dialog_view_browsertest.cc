// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_no_creds_dialog_view.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

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
    CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/false);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateAndShowDialog(const std::u16string& merchant_name,
                           bool show_opt_out) {
    dialog_view_ =
        (new SecurePaymentConfirmationNoCredsDialogView(this))->GetWeakPtr();

    model_ = std::make_unique<SecurePaymentConfirmationNoCredsModel>();
    model_->set_no_creds_text(merchant_name);
    model_->set_opt_out_visible(show_opt_out);
    model_->set_opt_out_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LABEL));
    model_->set_opt_out_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_LABEL));
    model_->set_relying_party_id(u"relyingparty.com");

    dialog_view_->ShowDialog(GetActiveWebContents(), model_->GetWeakPtr(),
                             base::DoNothing(), base::DoNothing());
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
  std::unique_ptr<SecurePaymentConfirmationNoCredsModel> model_;

  base::WeakPtr<SecurePaymentConfirmationNoCredsDialogView> dialog_view_;

  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       Default) {
  CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/false);
  EXPECT_THAT(base::UTF16ToUTF8(
                  GetLabelText(SecurePaymentConfirmationNoCredsDialogView::
                                   DialogViewID::NO_MATCHING_CREDS_TEXT)),
              HasSubstr("merchant.example"));
  // The no-creds dialog does not create the opt-out footnote by default.
  ASSERT_EQ(nullptr, dialog_view_->GetFootnoteViewForTesting());
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest, OptOut) {
  CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/true);

  // The opt-out link should be created and visible.
  views::View* opt_out_view = dialog_view_->GetFootnoteViewForTesting();
  EXPECT_NE(opt_out_view, nullptr);
  EXPECT_TRUE(opt_out_view->GetVisible());
  views::StyledLabel* opt_out_label =
      static_cast<views::StyledLabel*>(opt_out_view);

  // To avoid overfitting, we check only that the opt-out label contains both
  // the relying party and the call-to-action text that is expected.
  std::string opt_out_text = base::UTF16ToUTF8(opt_out_label->GetText());
  EXPECT_THAT(opt_out_text, ::testing::HasSubstr(
                                base::UTF16ToUTF8(model_->relying_party_id())));
  EXPECT_THAT(
      opt_out_text,
      ::testing::HasSubstr(base::UTF16ToUTF8(model_->opt_out_link_label())));

  // Now click the Opt Out link and make sure that the expected events occur.
  ResetEventWaiter(DialogEvent::OPT_OUT_CLICKED);
  opt_out_label->ClickFirstLinkForTesting();

  // If we make it past this wait, the delegate was correctly called.
  event_waiter_->Wait();
}

}  // namespace payments
