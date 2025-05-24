// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill_ai {

// This class displays a bubble to prompt the user whether they want save or
// update an AutofillAi entity.
class SaveOrUpdateAutofillAiDataBubbleView
    : public autofill::AutofillLocationBarBubble {
  METADATA_HEADER(SaveOrUpdateAutofillAiDataBubbleView, views::View)

 public:
  SaveOrUpdateAutofillAiDataBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      SaveOrUpdateAutofillAiDataController* controller);

  SaveOrUpdateAutofillAiDataBubbleView(
      const SaveOrUpdateAutofillAiDataBubbleView&) = delete;
  SaveOrUpdateAutofillAiDataBubbleView& operator=(
      const SaveOrUpdateAutofillAiDataBubbleView&) = delete;
  ~SaveOrUpdateAutofillAiDataBubbleView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  void WindowClosing() override;

 private:
  // Creates a row displayed in the dialog. This row contains information
  // about the entity to be saved or updated.
  std::unique_ptr<views::View> BuildEntityAttributeRow(
      const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
          detail);

  // Creates a view containing an entity attribute value. This method
  // also observes the created label. The observation is used to later
  // optionally update the text alignment depending on the number of lines the
  // rendered text has.
  std::unique_ptr<views::View> GetAttributeValueView(
      const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
          detail);

  void OnDialogAccepted();

  base::WeakPtr<SaveOrUpdateAutofillAiDataController> controller_;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_
