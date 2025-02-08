// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_

#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class EntityInstance;
}
namespace autofill_ai {

// Implementation of per-tab class to control the save Autofill AI data bubble.
// TODO(crbug.com/361434879): Introduce tests when this class has more than
// simple forwarding method.
class SaveAutofillAiDataControllerImpl
    : public autofill::AutofillBubbleControllerBase,
      public SaveAutofillAiDataController,
      public content::WebContentsUserData<SaveAutofillAiDataControllerImpl> {
 public:
  SaveAutofillAiDataControllerImpl(const SaveAutofillAiDataControllerImpl&) =
      delete;
  SaveAutofillAiDataControllerImpl& operator=(
      const SaveAutofillAiDataControllerImpl&) = delete;
  ~SaveAutofillAiDataControllerImpl() override;

  // SaveAutofillAiDataController:
  void OfferSave(autofill::EntityInstance autofill_ai_data,
                 AutofillAiClient::SavePromptAcceptanceCallback
                     save_prompt_acceptance_callback,
                 LearnMoreClickedCallback learn_more_clicked_callback) override;
  void OnSaveButtonClicked() override;
  base::optional_ref<const autofill::EntityInstance> GetAutofillAiData()
      const override;
  void OnBubbleClosed(AutofillAiBubbleClosedReason closed_reason) override;
  base::WeakPtr<SaveAutofillAiDataController> GetWeakPtr() override;
  void OnThumbsUpClicked() override;
  void OnThumbsDownClicked() override;
  void OnLearnMoreClicked() override;

 protected:
  explicit SaveAutofillAiDataControllerImpl(content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<SaveAutofillAiDataControllerImpl>;
  friend class SaveAutofillAiDataControllerImplTest;

  void ShowBubble();

  // Entity prompted to the user to be saved or updated.
  std::optional<autofill::EntityInstance> autofill_ai_data_;

  // Callback to notify the data provider about the user decision for the save
  // prompt.
  AutofillAiClient::SavePromptAcceptanceCallback
      save_prompt_acceptance_callback_;

  // Represents whether the user interacted with the thumbs up/down buttons.
  bool did_trigger_thumbs_up_ = false;
  bool did_trigger_thumbs_down_ = false;

  // Callback to notify that the user clicked the button to learn more about the
  // feature.
  LearnMoreClickedCallback learn_more_clicked_callback_;

  base::WeakPtrFactory<SaveAutofillAiDataControllerImpl> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_SAVE_AUTOFILL_AI_DATA_CONTROLLER_IMPL_H_
