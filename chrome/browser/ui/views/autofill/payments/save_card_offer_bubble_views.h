// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Combobox;
}  // namespace views

namespace autofill {

// This class displays the "Save credit card?" bubble that is shown when the
// user submits a form with a credit card number that Autofill has not
// previously saved. It includes a description of the card that is being saved
// and an [Save] button. (Non-material UI's include a [No Thanks] button).
class SaveCardOfferBubbleViews : public SaveCardBubbleViews,
                                 public views::TextfieldController {
  METADATA_HEADER(SaveCardOfferBubbleViews, SaveCardBubbleViews)
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardOfferBubbleViews(views::View* anchor_view,
                           content::WebContents* web_contents,
                           SaveCardBubbleController* controller);
  ~SaveCardOfferBubbleViews() override;

  SaveCardOfferBubbleViews(const SaveCardOfferBubbleViews&) = delete;
  SaveCardOfferBubbleViews& operator=(const SaveCardOfferBubbleViews&) = delete;

  // SaveCardBubbleViews:
  void Init() override;
  bool Accept() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  void AddedToWidget() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  std::unique_ptr<views::View> CreateMainContentView() override;
  std::unique_ptr<views::View> CreateRequestExpirationDateView();
  std::unique_ptr<views::View> CreateUploadExplanationView();
  std::unique_ptr<views::View> CreateLoadingRow();
  std::unique_ptr<LegalMessageView> CreateLegalMessageView();

  void LinkClicked(const GURL& url);
  void ShowThrobber();

  raw_ptr<views::Textfield> cardholder_name_textfield_ = nullptr;

  // Holds expiration inputs:
  raw_ptr<views::Combobox> month_input_dropdown_ = nullptr;
  raw_ptr<views::Combobox> year_input_dropdown_ = nullptr;
  MonthComboboxModel month_combobox_model_;
  YearComboboxModel year_combobox_model_;

  // loading_row_ is only set for upload card saves.
  raw_ptr<views::View> loading_row_ = nullptr;
  // loading_throbber_ is only used after upload save acceptance.
  raw_ptr<views::Throbber> loading_throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_
