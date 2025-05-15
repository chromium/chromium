// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
class EntityInstance;
}
namespace autofill_ai {

// Interface that exposes controller functionality to the save Autofill AI data
// bubble.
class SaveOrUpdateAutofillAiDataController {
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
    kMaxValue = kLostFocus
  };

  enum class EntityAttributeUpdateType {
    kNewEntityAttributeAdded,
    kNewEntityAttributeUpdated,
    kNewEntityAttributeUnchanged,
  };

  // Specifies for each attribute of a new instance whether the attribute is
  // new, updated, or unchanged. Also includes updates of an old instance
  // attribute that had its value changed.
  struct EntityAttributeUpdateDetails {
    EntityAttributeUpdateDetails();
    EntityAttributeUpdateDetails(std::u16string attribute_name,
                                 std::u16string attribute_value,
                                 EntityAttributeUpdateType update_type);
    EntityAttributeUpdateDetails(const EntityAttributeUpdateDetails&);
    EntityAttributeUpdateDetails(EntityAttributeUpdateDetails&&);
    EntityAttributeUpdateDetails& operator=(
        const EntityAttributeUpdateDetails&);
    EntityAttributeUpdateDetails& operator=(EntityAttributeUpdateDetails&&);
    ~EntityAttributeUpdateDetails();

    std::u16string attribute_name;
    std::u16string attribute_value;
    EntityAttributeUpdateType update_type{};
  };

  SaveOrUpdateAutofillAiDataController() = default;
  SaveOrUpdateAutofillAiDataController(
      const SaveOrUpdateAutofillAiDataController&) = delete;
  SaveOrUpdateAutofillAiDataController& operator=(
      const SaveOrUpdateAutofillAiDataController&) = delete;
  virtual ~SaveOrUpdateAutofillAiDataController() = default;

  static SaveOrUpdateAutofillAiDataController* GetOrCreate(
      content::WebContents* web_contents,
      const std::string& app_locale);

  // Shows a save or update Autofill AI data bubble which the user can accept or
  // decline. `old_entity` is used in the update case to give users an overview
  // of what was changed.
  virtual void ShowPrompt(autofill::EntityInstance new_entity,
                          std::optional<autofill::EntityInstance> old_entity,
                          AutofillAiClient::SaveOrUpdatePromptResultCallback
                              save_prompt_acceptance_callback) = 0;

  // Called when the user accepts to save or update Autofill AI data.
  virtual void OnSaveButtonClicked() = 0;

  virtual std::u16string GetDialogTitle() const = 0;

  // Returns images resource ids to be used in the dialog header. The first
  // value is to be used in a light mode theme and the second one in dark mode.
  virtual std::pair<int, int> GetTitleImagesResourceId() const = 0;

  // Returns details about the new/updated prompted entity. This is used by the
  // UI layer to give users details about what changes will be done if they
  // accept the prompt.
  virtual std::vector<EntityAttributeUpdateDetails>
  GetUpdatedAttributesDetails() const = 0;

  // Whether the prompt shown is for a new entity or whether it is an
  // update prompt.
  virtual bool IsSavePrompt() const = 0;

  // Returns the Autofill AI data to be displayed in the UI.
  virtual base::optional_ref<const autofill::EntityInstance> GetAutofillAiData()
      const = 0;

  // Called when the Autofill AI data bubble is closed.
  virtual void OnBubbleClosed(AutofillAiBubbleClosedReason closed_reason) = 0;

  virtual base::WeakPtr<SaveOrUpdateAutofillAiDataController> GetWeakPtr() = 0;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_H_
