// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class EntityInstance;

// Implementation of per-tab class to control the save or update Autofill AI
// data bubble.
// TODO(crbug.com/361434879): Introduce tests when this class has more than
// simple forwarding method.
class AutofillAiImportDataControllerImpl
    : public AutofillBubbleControllerBase,
      public AutofillAiImportDataController,
      public content::WebContentsUserData<AutofillAiImportDataControllerImpl> {
 public:
  AutofillAiImportDataControllerImpl(
      const AutofillAiImportDataControllerImpl&) = delete;
  AutofillAiImportDataControllerImpl& operator=(
      const AutofillAiImportDataControllerImpl&) = delete;
  ~AutofillAiImportDataControllerImpl() override;

  // AutofillAiImportDataController:
  void ShowPrompt(EntityInstance new_entity,
                  std::optional<EntityInstance> old_entity,
                  AutofillClient::EntityImportPromptResultCallback
                      prompt_closed_callback) override;
  void OnSaveButtonClicked() override;
  base::optional_ref<const EntityInstance> GetAutofillAiData() const override;
  void OnBubbleClosed(
      AutofillClient::AutofillAiBubbleClosedReason closed_reason) override;
  base::WeakPtr<AutofillAiImportDataController> GetWeakPtr() override;
  std::u16string GetDialogTitle() const override;
  std::u16string GetPrimaryAccountEmail() const override;
  bool IsWalletableEntity() const override;
  void OnGoToWalletLinkClicked() override;
  std::vector<EntityAttributeUpdateDetails> GetUpdatedAttributesDetails()
      const override;
  bool IsSavePrompt() const override;
  int GetTitleImagesResourceId() const override;

  // BubbleControllerBase:
  void OnBubbleDiscarded() override;
  BubbleType GetBubbleType() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

  // content::WebContentsObserver:
  // Used to re-show the bubble when it was previously closed due to the user
  // clicking on a link in the bubble.
  void OnVisibilityChanged(content::Visibility visibility) override;

 protected:
  explicit AutofillAiImportDataControllerImpl(
      content::WebContents* web_contents,
      const std::string& app_locale);

  // AutofillBubbleControllerBase::
  std::optional<PageActionIconType> GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<AutofillAiImportDataControllerImpl>;
  friend class AutofillAiImportDataControllerImplTest;

  // Configures the controller's state for the Autofill AI data
  // save/update/migrate prompt. `new_entity` is the data detected on the page,
  // `old_entity` is the existing data to be updated (if any), and
  // `prompt_closed_callback` is the callback to run upon user decision.
  void SetupPrompt(
      EntityInstance new_entity,
      std::optional<EntityInstance> old_entity,
      AutofillClient::EntityImportPromptResultCallback prompt_closed_callback);

  // The browser's locale when the object was instantiated.
  const std::string app_locale_;

  // New entity prompted to the user to be saved. It can be a fresh new entity
  // or an update of `old_entity`.
  std::optional<EntityInstance> new_entity_;

  // Present for updates. Used to give users information about which attribute
  // was either added or changed in their old instance.
  std::optional<EntityInstance> old_entity_;

  // Callback to notify the data provider about the user decision for the save
  // or update prompt.
  AutofillClient::EntityImportPromptResultCallback prompt_closed_callback_;

  // Whether the bubble should be re-shown when the current web_contents becomes
  // visible. This is true when the user has clicked a link in the bubble that
  // leads to a navigation. In situations like this the bubble is closed,
  // focusing back on the tab should re-open it.
  bool reopen_bubble_when_web_contents_becomes_visible_ = false;

  base::WeakPtrFactory<AutofillAiImportDataControllerImpl> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_IMPL_H_
