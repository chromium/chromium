// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEWS_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class BoxLayoutView;
class Combobox;
class Label;
class Textfield;
class Throbber;
}  // namespace views

namespace autofill {

class CardUnmaskPromptController;

class CardUnmaskPromptViews : public CardUnmaskPromptView,
                              public views::BubbleDialogDelegateView,
                              public views::TextfieldController {
  METADATA_HEADER(CardUnmaskPromptViews, views::BubbleDialogDelegateView)

 public:
  CardUnmaskPromptViews(CardUnmaskPromptController* controller,
                        content::WebContents* web_contents);
  CardUnmaskPromptViews(const CardUnmaskPromptViews&) = delete;
  CardUnmaskPromptViews& operator=(const CardUnmaskPromptViews&) = delete;
  ~CardUnmaskPromptViews() override;

  // CardUnmaskPromptView:
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override;

  // views::BubbleDialogDelegateView:
  View* GetContentsView() override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  bool Cancel() override;
  bool Accept() override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  friend class CardUnmaskPromptViewTesterViews;

  void InitIfNecessary();
  void SetRetriableErrorMessage(const std::u16string& message);
  bool ExpirationDateIsValid() const;
  void SetInputsEnabled(bool enabled);
  void ShowNewCardLink();
  void ClosePrompt();

  void UpdateButtons();

  void LinkClicked();

  void DateChanged();

  raw_ptr<CardUnmaskPromptController> controller_;
  base::WeakPtr<content::WebContents> web_contents_;

  // Expository language at the top of the dialog.
  raw_ptr<views::Label> instructions_ = nullptr;

  // Holds the cvc and expiration inputs.
  raw_ptr<View> input_row_ = nullptr;
  raw_ptr<views::Textfield> cvc_input_ = nullptr;
  raw_ptr<views::Combobox> month_input_ = nullptr;
  raw_ptr<views::Combobox> year_input_ = nullptr;

  MonthComboboxModel month_combobox_model_;
  YearComboboxModel year_combobox_model_;

  raw_ptr<views::View> new_card_link_ = nullptr;

  // The error row view and label for most errors, which live beneath the
  // inputs.
  raw_ptr<views::View> temporary_error_ = nullptr;
  raw_ptr<views::Label> error_label_ = nullptr;

  raw_ptr<views::View> controls_container_ = nullptr;

  // Elements related to progress or error when the request is being made.
  raw_ptr<views::BoxLayoutView> overlay_ = nullptr;
  raw_ptr<views::Label> overlay_label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;

  base::WeakPtrFactory<CardUnmaskPromptViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEWS_H_
