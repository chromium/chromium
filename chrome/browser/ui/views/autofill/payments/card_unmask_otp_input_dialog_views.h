// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_

#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class BoxLayoutView;
class Label;
class Throbber;
}  // namespace views

namespace autofill {

class CardUnmaskOtpInputDialogController;

class CardUnmaskOtpInputDialogViews : public CardUnmaskOtpInputDialogView,
                                      public views::DialogDelegateView,
                                      public views::TextfieldController {
 public:
  explicit CardUnmaskOtpInputDialogViews(
      base::WeakPtr<CardUnmaskOtpInputDialogController> controller);
  CardUnmaskOtpInputDialogViews(const CardUnmaskOtpInputDialogViews&) = delete;
  CardUnmaskOtpInputDialogViews& operator=(
      const CardUnmaskOtpInputDialogViews&) = delete;
  ~CardUnmaskOtpInputDialogViews() override;

  // Notifies the Card Unmask OTP Input Dialog Controller that the new code link
  // was clicked, and temporarily disables the link in the UI for
  // |kNewOtpCodeLinkDisabledDuration| (as defined in payments_ui_constants.h).
  void OnNewCodeLinkClicked();

  // When the new code link is clicked, it is instantly disabled, and re-enabled
  // after a delay of |kNewOtpCodeLinkDisabledDuration|. This is the function
  // that gets called after this delay to re-enable the new code link. This
  // function is abstracted here so that it can be referenced using the
  // |weak_ptr_factory_| from this class.
  void EnableNewCodeLink();

  // CardUnmaskOtpInputDialogView:
  void ShowPendingState() override;
  void ShowInvalidState(const std::u16string& invalid_label_text) override;
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override;
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  void AddedToWidget() override;
  bool Accept() override;
  views::View* GetInitiallyFocusedView() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

#if defined(UNIT_TEST)
  bool NewCodeLinkIsEnabledForTesting() {
    // The entire footer label gets set to not enabled when the new code link is
    // disabled, so footer_label_->GetEnabled() will always represent whether
    // the new code link is enabled or disabled.
    return footer_label_->GetEnabled();
  }

  void SetClosureToRunAfterNewCodeLinkIsEnabledForTesting(
      base::RepeatingClosure
          closure_to_run_after_new_code_link_is_enabled_for_testing) {
    closure_to_run_after_new_code_link_is_enabled_for_testing_ =
        closure_to_run_after_new_code_link_is_enabled_for_testing;
  }
#endif

 private:
  // Initializes the contents of the view and all of its child views.
  void InitViews();

  // Creates |otp_input_view_|, which is a child of the main view. Originally
  // set to visible as it is the first view of the dialog.
  void CreateOtpInputView();

  // Creates |progress_view_|, which is a child of the main view. Originally set
  // to not visible as it should only be visible once the user has submitted an
  // otp.
  void CreateHiddenProgressView();

  void HideInvalidState();

  void CloseWidget(bool user_closed_dialog, bool server_request_succeeded);

  // Sets the text and style of the dialog footer.
  void SetDialogFooter(bool enabled);

  base::WeakPtr<CardUnmaskOtpInputDialogController> controller_;

  // Elements related to the otp part of the view.
  raw_ptr<views::BoxLayoutView> otp_input_view_ = nullptr;
  raw_ptr<views::Textfield> otp_input_textfield_ = nullptr;
  raw_ptr<views::Label> otp_input_textfield_invalid_label_ = nullptr;
  raw_ptr<views::StyledLabel> footer_label_ = nullptr;

  // Adds padding to the view's layout so that the layout allows room for
  // |otp_input_textfield_invalid_label_| to appear if necessary. This padding's
  // visibility will always be the opposite of
  // |otp_input_textfield_invalid_label_|'s visibility.
  raw_ptr<views::View> otp_input_textfield_invalid_label_padding_ = nullptr;

  // Elements related to progress or error when the request is being made.
  raw_ptr<views::BoxLayoutView> progress_view_ = nullptr;
  raw_ptr<views::Label> progress_label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;

  base::RepeatingClosure
      closure_to_run_after_new_code_link_is_enabled_for_testing_;

  base::WeakPtrFactory<CardUnmaskOtpInputDialogViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_
