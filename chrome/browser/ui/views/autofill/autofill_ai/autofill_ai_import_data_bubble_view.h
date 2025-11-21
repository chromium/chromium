// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class StyledLabel;
}  // namespace views

namespace autofill {

// This class displays a bubble to prompt the user whether they want to save or
// update or migrate an AutofillAi entity.
class AutofillAiImportDataBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(AutofillAiImportDataBubbleView, views::View)

 public:
  AutofillAiImportDataBubbleView(views::BubbleAnchor anchor_view,
                                 content::WebContents* web_contents,
                                 AutofillAiImportDataController* controller);

  AutofillAiImportDataBubbleView(const AutofillAiImportDataBubbleView&) =
      delete;
  AutofillAiImportDataBubbleView& operator=(
      const AutofillAiImportDataBubbleView&) = delete;
  ~AutofillAiImportDataBubbleView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  void WindowClosing() override;

 private:
  // Creates a row displayed in the dialog. This row contains information
  // about the entity to be saved or updated.
  std::unique_ptr<views::View> BuildEntityAttributeRow(
      const AutofillAiImportDataController::EntityAttributeUpdateDetails&
          detail);


  void OnDialogAccepted() const;

  std::unique_ptr<views::Label> GetLocalEntitySubtitle() const;
  std::unique_ptr<views::StyledLabel> GetWalletableEntitySubtitle() const;

  base::WeakPtr<AutofillAiImportDataController> controller_;

  base::WeakPtrFactory<AutofillAiImportDataBubbleView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_BUBBLE_VIEW_H_
