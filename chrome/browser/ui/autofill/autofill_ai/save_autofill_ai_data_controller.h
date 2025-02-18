// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/user_annotations/user_annotations_types.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
class EntityInstance;
}
namespace autofill_ai {

// Interface that exposes controller functionality to the save Autofill AI data
// bubble.
class SaveAutofillAiDataController {
 public:

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
  // `old_entity` is used in the update case to give users an overview of what
  // was changed.
  virtual void OfferSave(autofill::EntityInstance new_entity,
                         std::optional<autofill::EntityInstance> old_entity,
                         AutofillAiClient::SavePromptAcceptanceCallback
                             save_prompt_acceptance_callback) = 0;

  // Called when the user accepts to save Autofill AI data.
  virtual void OnSaveButtonClicked() = 0;

  virtual std::u16string GetDialogTitle() const = 0;

  // Returns the Autofill AI data to be displayed in the UI.
  virtual base::optional_ref<const autofill::EntityInstance> GetAutofillAiData()
      const = 0;

  // Called when the Autofill AI data bubble is closed.
  virtual void OnBubbleClosed(AutofillAiBubbleClosedReason closed_reason) = 0;

  virtual base::WeakPtr<SaveAutofillAiDataController> GetWeakPtr() = 0;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_H_
