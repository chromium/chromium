// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"

#include <utility>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_prompt_view_android.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/messages_feature.h"

namespace autofill {

SaveUpdateAddressProfileFlowManager::SaveUpdateAddressProfileFlowManager() =
    default;
SaveUpdateAddressProfileFlowManager::~SaveUpdateAddressProfileFlowManager() =
    default;

void SaveUpdateAddressProfileFlowManager::OfferSave(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  DCHECK(web_contents);
  DCHECK(callback);

  // If the message or prompt is already shown, suppress the incoming offer.
  if (save_update_address_profile_message_controller_.IsMessageDisplayed() ||
      save_update_address_profile_prompt_controller_) {
    std::move(callback).Run(
        AutofillClient::AddressPromptUserDecision::kAutoDeclined, std::nullopt);
    return;
  }

  ShowConfirmationMessage(web_contents, profile, original_profile,
                          is_migration_to_account, std::move(callback));
}

SaveUpdateAddressProfileMessageController*
SaveUpdateAddressProfileFlowManager::GetMessageControllerForTest() {
  return &save_update_address_profile_message_controller_;
}

SaveUpdateAddressProfilePromptController*
SaveUpdateAddressProfileFlowManager::GetPromptControllerForTest() {
  return save_update_address_profile_prompt_controller_.get();
}

void SaveUpdateAddressProfileFlowManager::ShowConfirmationMessage(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  save_update_address_profile_message_controller_.DisplayMessage(
      web_contents, profile, original_profile, is_migration_to_account,
      std::move(callback),
      base::BindOnce(
          &SaveUpdateAddressProfileFlowManager::ShowPromptWithDetails,
          // Passing base::Unretained(this) is safe since |this|
          // owns the controller.
          base::Unretained(this)));
}

void SaveUpdateAddressProfileFlowManager::ShowPromptWithDetails(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  auto prompt_view_android =
      std::make_unique<SaveUpdateAddressProfilePromptViewAndroid>(web_contents);
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  save_update_address_profile_prompt_controller_ = std::make_unique<
      SaveUpdateAddressProfilePromptController>(
      std::move(prompt_view_android), personal_data, profile, original_profile,
      is_migration_to_account, std::move(callback),
      /*dismissal_callback=*/
      base::BindOnce(
          &SaveUpdateAddressProfileFlowManager::OnPromptWithDetailsDismissed,
          // Passing base::Unretained(this) is safe since |this|
          // owns the controller.
          base::Unretained(this)));
  save_update_address_profile_prompt_controller_->DisplayPrompt();
}

void SaveUpdateAddressProfileFlowManager::OnPromptWithDetailsDismissed() {
  save_update_address_profile_prompt_controller_.reset();
}

}  // namespace autofill
