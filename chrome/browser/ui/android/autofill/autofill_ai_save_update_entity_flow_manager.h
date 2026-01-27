// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_FLOW_MANAGER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_FLOW_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillAiSaveUpdateEntityPromptController;

// Class to manage save/update Autofill AI entities on Android. The flow
// consists of 3 steps:
// 1. showing a confirmation message
// 2. showing a prompt with profile details to review
// 3. showing a snackbar afterwards.
// This class owns and triggers the corresponding controllers.
class AutofillAiSaveUpdateEntityFlowManager {
 public:
  // Maximum number of lines this message's description can occupy.
  static constexpr int kDescriptionMaxLines = 2;

  explicit AutofillAiSaveUpdateEntityFlowManager(
      content::WebContents* web_contents,
      AutofillMessageController* autofill_message_controller,
      std::string app_locale);
  AutofillAiSaveUpdateEntityFlowManager(
      const AutofillAiSaveUpdateEntityFlowManager&) = delete;
  AutofillAiSaveUpdateEntityFlowManager& operator=(
      const AutofillAiSaveUpdateEntityFlowManager&) = delete;
  ~AutofillAiSaveUpdateEntityFlowManager();

  // Triggers a confirmation flow for saving or updating an Autofill AI entity.
  // If another flow is in progress, the incoming offer will be auto-declined.
  void OfferSave(
      EntityInstance entity,
      std::optional<EntityInstance> old_entity,
      AutofillClient::EntityImportPromptResultCallback prompt_result_callback);

 private:
  void OnMessagePrimaryAction(EntityInstance entity,
                              std::optional<EntityInstance> old_entity);

  void OnMessageDismissed(messages::DismissReason dismiss_reason);

  std::unique_ptr<AutofillMessageModel> CreateMessageModel(
      EntityInstance entity,
      std::optional<EntityInstance> old_entity);

  void RunPromptClosedCallback(AutofillClient::AutofillAiBubbleResult result);

  raw_ptr<content::WebContents> web_contents_;
  raw_ref<AutofillMessageController> autofill_message_controller_;
  std::unique_ptr<AutofillAiSaveUpdateEntityPromptController>
      save_update_entity_prompt_controller_;

  // Callback to notify the data provider about the user decision for the save
  // or update prompt.
  AutofillClient::EntityImportPromptResultCallback prompt_result_callback_;

  const std::string app_locale_;

  base::WeakPtrFactory<AutofillAiSaveUpdateEntityFlowManager> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_FLOW_MANAGER_H_
