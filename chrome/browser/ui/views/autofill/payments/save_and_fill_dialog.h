// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

class SaveAndFillDialogController;

// The dialog delegate view implementation for the Save and Fill dialog view.
// This is owned by the view hierarchy.
class SaveAndFillDialog : public views::DialogDelegateView,
                          public views::TextfieldController,
                          public views::FocusChangeListener {
 public:
  explicit SaveAndFillDialog(
      base::WeakPtr<SaveAndFillDialogController> controller,
      base::RepeatingCallback<void(const GURL&)> on_legal_message_link_clicked);
  SaveAndFillDialog(const SaveAndFillDialog&) = delete;
  SaveAndFillDialog& operator=(const SaveAndFillDialog&) = delete;
  ~SaveAndFillDialog() override;

  // DialogDelegateView:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnWidgetInitialized() override;

  std::u16string GetWindowTitle() const override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // views::FocusChangeListener
  void OnDidChangeFocus(views::View* before, views::View* now) override;

 private:
  // Initialize the dialog's contents.
  void InitViews();
  // Extract user-provided card details from the textfields in the Save and Fill
  // dialog.
  payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
  GetUserProvidedDataFromInput() const;
  // Callback that is triggered when the dialog is canceled.
  void OnDialogClosed(views::Widget::ClosedReason reason);
  // Callback for when the accept button is clicked.
  bool OnAccepted();
  // Create a view with a legal message.
  std::unique_ptr<views::View> CreateLegalMessageView();

  void CreateMainContentView();
  void CreatePendingView();
  void ToggleThrobberVisibility(bool visible);

  base::WeakPtr<SaveAndFillDialogController> controller_;
  base::RepeatingCallback<void(const GURL&)> on_legal_message_link_clicked_;

  // The focus manager associated with this view. The focus manager is expected
  // to outlive this view.
  raw_ptr<views::FocusManager> focus_manager_ = nullptr;

  LabeledTextfieldWithErrorMessage card_number_data_;
  LabeledTextfieldWithErrorMessage cvc_data_;
  LabeledTextfieldWithErrorMessage expiration_date_data_;
  LabeledTextfieldWithErrorMessage name_on_card_data_;

  raw_ptr<views::View> container_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> main_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> pending_view_ = nullptr;
  raw_ptr<views::Throbber> throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_DIALOG_H_
