// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_ai_delegate.h"
#include "components/user_annotations/user_annotations_types.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide::proto {
class UserAnnotationsEntry;
}

namespace autofill_ai {

// Interface that exposes controller functionality to the save Autofill AI data
// bubble.
class SaveAutofillAiDataController {
 public:
  using LearnMoreClickedCallback = base::RepeatingCallback<void()>;
  using UserFeedbackCallback =
      base::RepeatingCallback<void(autofill::AutofillAiDelegate::UserFeedback)>;

  enum class AutofillAiBubbleClosedReason {
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

  SaveAutofillAiDataController() = default;
  SaveAutofillAiDataController(const SaveAutofillAiDataController&) = delete;
  SaveAutofillAiDataController& operator=(const SaveAutofillAiDataController&) =
      delete;
  virtual ~SaveAutofillAiDataController() = default;

  static SaveAutofillAiDataController* GetOrCreate(
      content::WebContents* web_contents);

  // Shows a save Autofill AI data bubble which the user can accept or decline.
  virtual void OfferSave(
      std::vector<optimization_guide::proto::UserAnnotationsEntry>
          prediction_improvements,
      user_annotations::PromptAcceptanceCallback prompt_acceptance_callback,
      LearnMoreClickedCallback learn_more_clicked_callback,
      UserFeedbackCallback user_feedback_callback) = 0;

  // Called when the user accepts to save Autofill AI data.
  virtual void OnSaveButtonClicked() = 0;

  // Called when the user clicks on the thumbs up button in the dialog.
  virtual void OnThumbsUpClicked() = 0;

  // Called when the user clicks on the thumbs down button in the dialog.
  virtual void OnThumbsDownClicked() = 0;

  // Called when the user clicks on the learn more button in the dialog.
  virtual void OnLearnMoreClicked() = 0;

  // Returns the Autofill AI data to be displayed in the UI.
  virtual const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
  GetAutofillAiData() const = 0;

  // Called when the Autofill AI data bubble is closed.
  virtual void OnBubbleClosed(AutofillAiBubbleClosedReason closed_reason) = 0;

  virtual base::WeakPtr<SaveAutofillAiDataController> GetWeakPtr() = 0;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_
