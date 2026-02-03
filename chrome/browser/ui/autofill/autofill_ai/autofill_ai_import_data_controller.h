// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class EntityInstance;

// Interface that exposes controller functionality to the save/update/migrate
// Autofill AI data bubble.
class AutofillAiImportDataController {
 public:
  AutofillAiImportDataController() = default;
  AutofillAiImportDataController(const AutofillAiImportDataController&) =
      delete;
  AutofillAiImportDataController& operator=(
      const AutofillAiImportDataController&) = delete;
  virtual ~AutofillAiImportDataController() = default;

  static AutofillAiImportDataController* GetOrCreate(
      content::WebContents* web_contents,
      const std::string& app_locale);

  // Hides the Autofill AI import bubble if it is showing for `web_contents`.
  static void Hide(content::WebContents& web_contents);

  // Shows a save or update Autofill AI data bubble which the user can accept or
  // decline. `old_entity` is used in the update case to give users an overview
  // of what was changed.
  virtual void ShowPrompt(EntityInstance new_entity,
                          std::optional<EntityInstance> old_entity,
                          bool close_on_accept,
                          AutofillClient::EntityImportPromptResultCallback
                              prompt_result_callback) = 0;

  // Show  a notification that the entity could not be saved to Wallet and
  // instead was saved locally.
  virtual void ShowLocalSaveNotification() = 0;

  virtual base::WeakPtr<AutofillAiImportDataController> GetWeakPtr() = 0;

  // The following methods are related to the save or update Autofill AI data
  // bubble and should only be called when such a bubble is showing.
  // The code CHECKs this precondition:

  // Called when the user accepts to save or update Autofill AI data.
  virtual void OnSaveButtonClicked() = 0;

  virtual std::u16string GetSaveUpdateDialogPrimaryButtonText() const = 0;
  virtual std::u16string GetSaveUpdateDialogTitle() const = 0;

  // Returns an image resource id to be used in the dialog header.
  virtual int GetSaveUpdateDialogTitleImagesResourceId() const = 0;

  // Returns the user's primary account email.
  virtual std::u16string GetPrimaryAccountEmail() const = 0;

  // Returns true if the entity to be saved or updated will be stored in the
  // wallet server.
  virtual bool IsWalletableEntity() const = 0;

  // Whether the user clicked the link the dialog subtitle which navigates them
  // to wallet.
  virtual void OnGoToWalletLinkClicked() = 0;

  // Returns details about the new/updated prompted entity. This is used by the
  // UI layer to give users details about what changes will be done if they
  // accept the prompt.
  virtual std::vector<EntityAttributeUpdateDetails>
  GetUpdatedAttributesDetails() const = 0;

  // Whether the prompt shown is for a new entity or whether it is an
  // update prompt.
  virtual bool IsSavePrompt() const = 0;

  // Returns the Autofill AI data to be displayed in the UI.
  virtual base::optional_ref<const EntityInstance> GetAutofillAiData()
      const = 0;

  // Whether the bubble should be closed when the user accepts the prompt.
  virtual bool CloseOnAccept() const = 0;

  // Called when the Autofill AI save or update bubble is closed.
  virtual void OnBubbleClosed(
      AutofillClient::AutofillAiBubbleResult result) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_
