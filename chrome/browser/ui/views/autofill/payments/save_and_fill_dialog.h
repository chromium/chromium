// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class SaveAndFillDialogController;

// The dialog delegate view implementation for the Save and Fill dialog view.
// This is owned by the view hierarchy.
class SaveAndFillDialog : public views::DialogDelegateView,
                          public views::TextfieldController {
 public:
  explicit SaveAndFillDialog(
      base::WeakPtr<SaveAndFillDialogController> controller);
  SaveAndFillDialog(const SaveAndFillDialog&) = delete;
  SaveAndFillDialog& operator=(const SaveAndFillDialog&) = delete;
  ~SaveAndFillDialog() override;

  // DialogDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  // Initialize the dialog's contents.
  void InitViews();

  base::WeakPtr<SaveAndFillDialogController> controller_;

  LabeledTextfieldWithErrorMessage card_number_data_;
  LabeledTextfieldWithErrorMessage cvc_data_;
  LabeledTextfieldWithErrorMessage expiration_date_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_
