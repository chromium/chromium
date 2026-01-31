// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_prompt_view_android.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveUpdateAddressProfileFlowManager::SaveUpdateAddressProfileFlowManager(
    ContentAutofillClient* owner,
    AutofillMessageController* autofill_message_controller)
    : owner_(CHECK_DEREF(owner)),
      autofill_message_controller_(CHECK_DEREF(autofill_message_controller)) {}

SaveUpdateAddressProfileFlowManager::~SaveUpdateAddressProfileFlowManager() =
    default;

void SaveUpdateAddressProfileFlowManager::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveUpdateAddressProfilePromptMode prompt_mode,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  DCHECK(callback);

  // If the message is already shown, suppress the incoming offer.
  if (is_message_displayed_ || save_update_address_profile_prompt_controller_) {
    std::move(callback).Run(
        AutofillClient::AddressPromptUserDecision::kAutoDeclined, std::nullopt);
    return;
  }

  ShowConfirmationMessage(profile, original_profile, prompt_mode,
                          std::move(callback));
}

void SaveUpdateAddressProfileFlowManager::ShowConfirmationMessage(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveUpdateAddressProfilePromptMode prompt_mode,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  profile_ = profile;
  original_profile_ = original_profile;
  prompt_mode_ = prompt_mode;
  save_address_profile_callback_ = std::move(callback);
  is_message_displayed_ = true;

  autofill_message_controller_->Show(CreateMessageModel());
}

void SaveUpdateAddressProfileFlowManager::OnMessagePrimaryAction() {
  ShowPromptWithDetails(profile_.value(), original_profile_, prompt_mode_,
                        std::move(save_address_profile_callback_));
}

void SaveUpdateAddressProfileFlowManager::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  is_message_displayed_ = false;

  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
      // Primary action is handled separately, no secondary action.
      break;
    case messages::DismissReason::GESTURE:
      // User explicitly dismissed the message.
      RunSaveAddressProfileCallback(
          AutofillClient::AddressPromptUserDecision::kMessageDeclined);
      break;
    case messages::DismissReason::TIMER:
      // The message was auto-dismissed after a timeout.
      RunSaveAddressProfileCallback(
          AutofillClient::AddressPromptUserDecision::kMessageTimeout);
      break;
    default:
      // Dismissal for any other reason.
      RunSaveAddressProfileCallback(
          AutofillClient::AddressPromptUserDecision::kIgnored);
  }
}

void SaveUpdateAddressProfileFlowManager::RunSaveAddressProfileCallback(
    AutofillClient::AddressPromptUserDecision decision) {
  std::move(save_address_profile_callback_).Run(decision, std::nullopt);
}

std::unique_ptr<AutofillMessageModel>
SaveUpdateAddressProfileFlowManager::CreateMessageModel() {
  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::SAVE_ADDRESS_PROFILE);

  message->SetTitle(GetMessageTitle());
  message->SetDescription(GetMessageDescription());
  message->SetDescriptionMaxLines(kMessageDescriptionMaxLines);
  message->SetPrimaryButtonText(GetMessagePrimaryButtonText());
  message->SetPrimaryButtonTextMaxLines(1);
  message->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IsMigrationToAccount() ? IDR_ANDROID_AUTOFILL_UPLOAD_ADDRESS
                             : IDR_ANDROID_AUTOFILL_ADDRESS));

  return std::make_unique<AutofillMessageModel>(
      std::move(message), AutofillMessageModel::Type::kAddressSaveUpdateFlow,
      base::BindOnce(
          &SaveUpdateAddressProfileFlowManager::OnMessagePrimaryAction,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SaveUpdateAddressProfileFlowManager::OnMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::u16string SaveUpdateAddressProfileFlowManager::GetMessageTitle() const {
  if (original_profile_ && !original_profile_->IsHomeAndWorkProfile()) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  }

  if (IsMigrationToAccount()) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
}

std::u16string SaveUpdateAddressProfileFlowManager::GetMessageDescription()
    const {
  if (original_profile_) {
    // Return description without source notice for profile update.
    return GetProfileDescription(*original_profile_,
                                 g_browser_process->GetApplicationLocale(),
                                 /*include_address_and_contacts=*/true);
  }

  if (IsMigrationToAccount() || profile_->IsAccountProfile()) {
    return GetMessageRecordTypeNotice();
  }

  // Address profile won't be saved to Google Account when user is not logged
  // in.
  return GetProfileDescription(*profile_,
                               g_browser_process->GetApplicationLocale(),
                               /*include_address_and_contacts=*/true);
}

std::u16string SaveUpdateAddressProfileFlowManager::GetMessageRecordTypeNotice()
    const {
  std::optional<AccountInfo> account = GetPrimaryAccountInfoFromBrowserContext(
      owner_->GetWebContents().GetBrowserContext());
  if (!account) {
    return std::u16string();
  }

  return IsMigrationToAccount()
             ? l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_MIGRATION_RECORD_TYPE_NOTICE)
             : l10n_util::GetStringFUTF16(
                   IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_RECORD_TYPE_NOTICE,
                   base::UTF8ToUTF16(account->email));
}

std::u16string
SaveUpdateAddressProfileFlowManager::GetMessagePrimaryButtonText() const {
  if (original_profile_ && !original_profile_->IsHomeAndWorkProfile()) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

void SaveUpdateAddressProfileFlowManager::ShowPromptWithDetails(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveUpdateAddressProfilePromptMode prompt_mode,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          owner_->GetWebContents().GetBrowserContext());
  save_update_address_profile_prompt_controller_ = std::make_unique<
      SaveUpdateAddressProfilePromptController>(
      CreatePromptView(), personal_data, profile, original_profile, prompt_mode,
      std::move(callback),
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

std::unique_ptr<SaveUpdateAddressProfilePromptView>
SaveUpdateAddressProfileFlowManager::CreatePromptView() {
  return std::make_unique<SaveUpdateAddressProfilePromptViewAndroid>(
      &owner_->GetWebContents());
}

bool SaveUpdateAddressProfileFlowManager::IsMigrationToAccount() const {
  switch (prompt_mode_) {
    case SaveUpdateAddressProfilePromptMode::kMigrateProfile:
      return true;
    case SaveUpdateAddressProfilePromptMode::kCreateNewProfile:
    case SaveUpdateAddressProfilePromptMode::kSaveNewProfile:
    case SaveUpdateAddressProfilePromptMode::kUpdateProfile:
      return false;
  }
  NOTREACHED();
}

}  // namespace autofill
