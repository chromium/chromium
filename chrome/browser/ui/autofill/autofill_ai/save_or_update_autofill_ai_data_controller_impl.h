// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class EntityInstance;
}
namespace autofill_ai {

// Implementation of per-tab class to control the save or update Autofill AI
// data bubble.
// TODO(crbug.com/361434879): Introduce tests when this class has more than
// simple forwarding method.
class SaveOrUpdateAutofillAiDataControllerImpl
    : public autofill::AutofillBubbleControllerBase,
      public SaveOrUpdateAutofillAiDataController,
      public content::WebContentsUserData<
          SaveOrUpdateAutofillAiDataControllerImpl> {
 public:
  SaveOrUpdateAutofillAiDataControllerImpl(
      const SaveOrUpdateAutofillAiDataControllerImpl&) = delete;
  SaveOrUpdateAutofillAiDataControllerImpl& operator=(
      const SaveOrUpdateAutofillAiDataControllerImpl&) = delete;
  ~SaveOrUpdateAutofillAiDataControllerImpl() override;

  // SaveOrUpdateAutofillAiDataController:
  void ShowPrompt(autofill::EntityInstance new_entity,
                  std::optional<autofill::EntityInstance> old_entity,
                  AutofillAiClient::SaveOrUpdatePromptResultCallback
                      save_prompt_acceptance_callback) override;
  void OnSaveButtonClicked() override;
  base::optional_ref<const autofill::EntityInstance> GetAutofillAiData()
      const override;
  void OnBubbleClosed(AutofillAiBubbleClosedReason closed_reason) override;
  base::WeakPtr<SaveOrUpdateAutofillAiDataController> GetWeakPtr() override;
  std::u16string GetDialogTitle() const override;
  std::vector<EntityAttributeUpdateDetails> GetUpdatedAttributesDetails()
      const override;
  bool IsSavePrompt() const override;
  std::pair<int, int> GetTitleImagesResourceId() const override;

 protected:
  explicit SaveOrUpdateAutofillAiDataControllerImpl(
      content::WebContents* web_contents,
      const std::string& app_locale);

  // AutofillBubbleControllerBase::
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      SaveOrUpdateAutofillAiDataControllerImpl>;
  friend class SaveOrUpdateAutofillAiDataControllerImplTest;

  void ShowBubble();

  // The browser's locale when the object was instantiated.
  const std::string app_locale_;

  // New entity prompted to the user to be saved. It can be a fresh new entity
  // or an update of `old_entity`.
  std::optional<autofill::EntityInstance> new_entity_;

  // Present for updates. Used to give users information about which attribute
  // was either added or changed in their old instance.
  std::optional<autofill::EntityInstance> old_entity_;

  // Callback to notify the data provider about the user decision for the save
  // or update prompt.
  AutofillAiClient::SaveOrUpdatePromptResultCallback
      save_prompt_acceptance_callback_;

  base::WeakPtrFactory<SaveOrUpdateAutofillAiDataControllerImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_
