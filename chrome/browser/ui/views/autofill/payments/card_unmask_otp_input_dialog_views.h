// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_

#include <string>

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_view.h"
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
      CardUnmaskOtpInputDialogController* controller);
  CardUnmaskOtpInputDialogViews(const CardUnmaskOtpInputDialogViews&) = delete;
  CardUnmaskOtpInputDialogViews& operator=(
      const CardUnmaskOtpInputDialogViews&) = delete;
  ~CardUnmaskOtpInputDialogViews() override;

  // CardUnmaskOtpInputDialogView:
  void ShowPendingState() override;
  void ShowErrorMessage(const std::u16string error_message) override;
  void OnControllerDestroying() override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  void AddedToWidget() override;
  bool Accept() override;
  void OnThemeChanged() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

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

  CardUnmaskOtpInputDialogController* controller_ = nullptr;

  // Elements related to the otp part of the view.
  views::BoxLayoutView* otp_input_view_ = nullptr;
  views::Textfield* otp_input_textfield_ = nullptr;

  // Elements related to progress or error when the request is being made.
  views::BoxLayoutView* progress_view_ = nullptr;
  views::Label* progress_label_ = nullptr;
  views::Throbber* progress_throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEWS_H_
