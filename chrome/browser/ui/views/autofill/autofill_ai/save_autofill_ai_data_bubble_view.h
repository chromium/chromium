// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill_ai {

class SaveAutofillAiDataController;

// This class displays a bubble to prompt the user whether they want to store
// improved predictions.
class SaveAutofillAiDataBubbleView
    : public autofill::AutofillLocationBarBubble {
  METADATA_HEADER(SaveAutofillAiDataBubbleView, views::View)

 public:
  static constexpr int kLearnMoreStyledLabelViewID = 436;
  static constexpr int kThumbsUpButtonViewID = 437;
  static constexpr int kThumbsDownButtonViewID = 438;

  SaveAutofillAiDataBubbleView(views::View* anchor_view,
                               content::WebContents* web_contents,
                               SaveAutofillAiDataController* controller);

  SaveAutofillAiDataBubbleView(const SaveAutofillAiDataBubbleView&) = delete;
  SaveAutofillAiDataBubbleView& operator=(const SaveAutofillAiDataBubbleView&) =
      delete;
  ~SaveAutofillAiDataBubbleView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  void WindowClosing() override;

 private:
  void OnDialogAccepted();

  base::WeakPtr<SaveAutofillAiDataController> controller_;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_BUBBLE_VIEW_H_
