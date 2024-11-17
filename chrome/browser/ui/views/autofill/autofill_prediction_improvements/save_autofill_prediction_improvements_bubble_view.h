// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill {

class SaveAutofillPredictionImprovementsController;

// This class displays a bubble to prompt the user whether they want to store
// improved predictions.
class SaveAutofillPredictionImprovementsBubbleView
    : public AutofillLocationBarBubble {
  METADATA_HEADER(SaveAutofillPredictionImprovementsBubbleView, views::View)

 public:
  static constexpr int kLearnMoreStyledLabelViewID = 436;
  static constexpr int kThumbsUpButtonViewID = 437;
  static constexpr int kThumbsDownButtonViewID = 438;

  SaveAutofillPredictionImprovementsBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      SaveAutofillPredictionImprovementsController* controller);

  SaveAutofillPredictionImprovementsBubbleView(
      const SaveAutofillPredictionImprovementsBubbleView&) = delete;
  SaveAutofillPredictionImprovementsBubbleView& operator=(
      const SaveAutofillPredictionImprovementsBubbleView&) = delete;
  ~SaveAutofillPredictionImprovementsBubbleView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  void WindowClosing() override;

 private:
  void OnDialogAccepted();

  base::WeakPtr<SaveAutofillPredictionImprovementsController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_BUBBLE_VIEW_H_
