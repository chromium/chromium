// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Interface that exposes controller functionality to save prediction
// improvements bubble.
class SaveAutofillPredictionImprovementsController {
 public:
  enum class PredictionImprovementsBubbleClosedReason {
    // Bubble closed reason not specified.
    kUnknown,
    // The user explicitly accepted the bubble.
    kAccepted,
    // The user explicitly cancelled the bubble.
    kCancelled,
    // The user explicitly closed the bubble (via the close button or the ESC).
    kClosed,
    // The bubble was not interacted with.
    kNotInteracted,
    // The bubble lost focus and was closed.
    kLostFocus,
  };
  struct PredictionImprovement {
    // The prediction key displayed to the user and also used to identify it.
    std::u16string key;
    // The value of the prediction.
    std::u16string value;
  };

  SaveAutofillPredictionImprovementsController() = default;
  SaveAutofillPredictionImprovementsController(
      const SaveAutofillPredictionImprovementsController&) = delete;
  SaveAutofillPredictionImprovementsController& operator=(
      const SaveAutofillPredictionImprovementsController&) = delete;
  virtual ~SaveAutofillPredictionImprovementsController() = default;

  static SaveAutofillPredictionImprovementsController* GetOrCreate(
      content::WebContents* web_contents);

  // Shows a save improved predictions bubble which the user can accept or
  // decline.
  virtual void OfferSave(
      std::vector<PredictionImprovement> prediction_improvements) = 0;

  // Called when the user accepts to save prediction improvements.
  virtual void OnSaveButtonClicked() = 0;

  // Returns the prediction improvements to be displayed in the UI.
  virtual const std::vector<PredictionImprovement>& GetPredictionImprovements()
      const = 0;

  // Called when the prediction improvements bubble is closed.
  virtual void OnBubbleClosed(
      PredictionImprovementsBubbleClosedReason closed_reason) = 0;

  virtual base::WeakPtr<SaveAutofillPredictionImprovementsController>
  GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_H_
