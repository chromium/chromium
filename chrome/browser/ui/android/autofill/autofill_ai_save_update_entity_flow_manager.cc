// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_flow_manager.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_prompt_view_android.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_prompt_view_android.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/messages/android/message_enums.h"
#include "components/resources/android/theme_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::u16string GetMessageDescription(content::WebContents* web_contents,
                                     bool is_wallet_entity) {
  if (!is_wallet_entity) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_AI_SAVE_ENTITY_MESSAGE_SUBTITLE);
  }
  std::optional<AccountInfo> account = GetPrimaryAccountInfoFromBrowserContext(
      web_contents->GetBrowserContext());
  if (!account) {
    return std::u16string();
  }

  const std::u16string google_wallet_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_MESSAGE_SUBTITLE,
      google_wallet_text, base::UTF8ToUTF16(account->email));
}

int GetMessageIconResourceId(const EntityInstance& entity) {
  if (entity.record_type() == EntityInstance::RecordType::kServerWallet) {
    return IDR_ANDROID_AUTOFILL_WALLET;
  }
  switch (entity.type().name()) {
    case EntityTypeName::kDriversLicense:
      return IDR_ANDROID_AUTOFILL_ID_CARD;
    case EntityTypeName::kKnownTravelerNumber:
      return IDR_ANDROID_AUTOFILL_TRAVEL_LUGGAGE_AND_BAGS;
    case EntityTypeName::kNationalIdCard:
      return IDR_ANDROID_AUTOFILL_ID_CARD;
    case EntityTypeName::kPassport:
      return IDR_ANDROID_AUTOFILL_PASSPORT;
    case EntityTypeName::kRedressNumber:
      return IDR_ANDROID_AUTOFILL_TRAVEL_LUGGAGE_AND_BAGS;
    case EntityTypeName::kVehicle:
      return IDR_ANDROID_AUTOFILL_VEHICLE;
    case EntityTypeName::kFlightReservation:
      NOTREACHED() << "Entity is read only and doesn't support save prompts.";
    case EntityTypeName::kOrder:
      NOTREACHED() << "Entity is read only and doesn't support save prompts.";
  }
  NOTREACHED();
}

}  // namespace

AutofillAiSaveUpdateEntityFlowManager::AutofillAiSaveUpdateEntityFlowManager(
    content::WebContents* web_contents,
    AutofillMessageController* autofill_message_controller,
    std::string app_locale)
    : web_contents_(web_contents),
      autofill_message_controller_(CHECK_DEREF(autofill_message_controller)),
      app_locale_(std::move(app_locale)) {}

AutofillAiSaveUpdateEntityFlowManager::
    ~AutofillAiSaveUpdateEntityFlowManager() = default;

void AutofillAiSaveUpdateEntityFlowManager::OfferSave(
    EntityInstance entity,
    std::optional<EntityInstance> old_entity,
    AutofillClient::EntityImportPromptResultCallback prompt_result_callback) {
  if (prompt_result_callback_) {
    return;
  }
  prompt_result_callback_ = std::move(prompt_result_callback);
  autofill_message_controller_->Show(
      CreateMessageModel(std::move(entity), std::move(old_entity)));
}

std::unique_ptr<AutofillMessageModel>
AutofillAiSaveUpdateEntityFlowManager::CreateMessageModel(
    EntityInstance entity,
    std::optional<EntityInstance> old_entity) {
  // Binding with base::Unretained(this) is safe here because
  // AutofillAiSaveUpdateEntityMessageController owns message_. Callbacks won't
  // be called after the current object is destroyed.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_UPDATE_ENTITY);

  message->SetTitle(GetPromptTitle(entity.type().name(), !old_entity));
  message->SetDescription(GetMessageDescription(
      web_contents_,
      entity.record_type() == EntityInstance::RecordType::kServerWallet));
  message->SetDescriptionMaxLines(kDescriptionMaxLines);
  message->SetPrimaryButtonText(GetPrimaryButtonText(!old_entity));
  message->SetPrimaryButtonTextMaxLines(1);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(GetMessageIconResourceId(entity)));
  // TODO: crbug.com/460410690 - Fix the tinting issue for unbranded icons.
  message->DisableIconTint();

  return std::make_unique<AutofillMessageModel>(
      std::move(message), AutofillMessageModel::Type::kEntitySaveUpdateFlow,
      base::BindOnce(
          &AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction,
          weak_ptr_factory_.GetWeakPtr(), std::move(entity),
          std::move(old_entity)),
      base::BindOnce(&AutofillAiSaveUpdateEntityFlowManager::OnMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction(
    EntityInstance entity,
    std::optional<EntityInstance> old_entity) {
  auto prompt_view_android =
      std::make_unique<AutofillAiSaveUpdateEntityPromptViewAndroid>(
          web_contents_);
  save_update_entity_prompt_controller_ =
      std::make_unique<AutofillAiSaveUpdateEntityPromptController>(
          web_contents_, std::move(prompt_view_android), std::move(entity),
          std::move(old_entity), app_locale_,
          std::move(prompt_result_callback_));
  save_update_entity_prompt_controller_->DisplayPrompt();
}

void AutofillAiSaveUpdateEntityFlowManager::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
      // Primary action is handled separately, no secondary action.
      break;
    case messages::DismissReason::GESTURE:
      // User explicitly dismissed the message.
      RunPromptClosedCallback(AutofillClient::AutofillAiBubbleResult::kClosed);
      break;
    default:
      // Dismissal for any other reason like timeout, tab switch, etc.
      RunPromptClosedCallback(
          AutofillClient::AutofillAiBubbleResult::kNotInteracted);
      break;
  }
}

void AutofillAiSaveUpdateEntityFlowManager::RunPromptClosedCallback(
    AutofillClient::AutofillAiBubbleResult result) {
  if (prompt_result_callback_) {
    std::move(prompt_result_callback_).Run(result);
  }
}

}  // namespace autofill
