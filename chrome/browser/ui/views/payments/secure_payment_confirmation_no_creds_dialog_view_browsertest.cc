// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_no_creds_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/window/dialog_client_view.h"

namespace payments {

using ::testing::HasSubstr;

class SecurePaymentConfirmationNoCredsDialogViewTest
    : public DialogBrowserTest,
      public SecurePaymentConfirmationNoCredsDialogView::ObserverForTest {
 public:
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

  void ClickButton(views::View* button) {
    gfx::Point center(button->width() / 2, button->height() / 2);
    const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
    button->OnMousePressed(event);
    button->OnMouseReleased(event);
  }

  // SecurePaymentConfirmationNoCredsDialogView::ObserverForTest
  void OnDialogClosed() override { dialog_closed_ = true; }
  void OnOptOutClicked() override { opt_out_clicked_ = true; }

 protected:
  std::unique_ptr<SecurePaymentConfirmationNoCredsModel> model_;

  base::WeakPtr<SecurePaymentConfirmationNoCredsDialogView> dialog_view_;

  bool dialog_closed_ = false;
  bool opt_out_clicked_ = false;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       ViewMatchesModel) {
  CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/false);
  EXPECT_THAT(base::UTF16ToUTF8(
                  GetLabelText(SecurePaymentConfirmationNoCredsDialogView::
                                   DialogViewID::NO_MATCHING_CREDS_TEXT)),
              HasSubstr("merchant.example"));
  // The no-creds dialog does not create the opt-out footnote by default.
  ASSERT_EQ(nullptr, dialog_view_->GetFootnoteViewForTesting());

  // The dialog should have an Ok button but no Cancel button.
  ASSERT_TRUE(dialog_view_->GetOkButton());
  ASSERT_FALSE(dialog_view_->GetCancelButton());
}

// Test that the 'Continue' button is protected against accidental inputs.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationNoCredsDialogViewTest,
                       ContinueButtonIgnoresAccidentalInputs) {
  CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/false);

  // Insert a mock input protector that will ignore the first input and then
  // accepts all subsequent inputs.
  auto mock_input_protector =
      std::make_unique<views::MockInputEventActivationProtector>();
  EXPECT_CALL(*mock_input_protector, IsPossiblyUnintendedInteraction)
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));
  dialog_view_->GetDialogClientView()->SetInputProtectorForTesting(
      std::move(mock_input_protector));

  // Because of the input protector, the first press of the button should be
  // ignored.
  ClickButton(dialog_view_->GetOkButton());
  EXPECT_FALSE(dialog_closed_);

  // However a subsequent press should be accepted.
  ClickButton(dialog_view_->GetOkButton());
  EXPECT_TRUE(dialog_closed_);
}

// Test that the opt out link is shown when requested, and that clicking it
// correctly closes the dialog via the expected path.
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
  opt_out_label->ClickFirstLinkForTesting();
  EXPECT_TRUE(opt_out_clicked_);
}

class SecurePaymentConfirmationNoCredsDialogViewWithInlineNetworkAndIssuerTest
    : public SecurePaymentConfirmationNoCredsDialogViewTest {
 public:
  SecurePaymentConfirmationNoCredsDialogViewWithInlineNetworkAndIssuerTest() {
    base::FieldTrialParams params;
    params["spc_network_and_issuer_icons_option"] = "inline";
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kSecurePaymentConfirmationNetworkAndIssuerIcons,
        params);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the cart icon is still shown even when the inline network/issuer
// flag is set.
IN_PROC_BROWSER_TEST_F(
    SecurePaymentConfirmationNoCredsDialogViewWithInlineNetworkAndIssuerTest,
    CartIconStillShows) {
  CreateAndShowDialog(u"merchant.example", /*show_opt_out=*/false);

  EXPECT_NE(nullptr, dialog_view_->GetViewByID(static_cast<int>(
                         SecurePaymentConfirmationNoCredsDialogView::
                             DialogViewID::HEADER_ICON)));
}

}  // namespace payments
