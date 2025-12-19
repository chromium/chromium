// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_flow_manager.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"
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
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::u16string GetMessageDescription() {
  // TODO: crbug.com/460410690 - Confirm save/update subtitle strings.
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_SAVE_ENTITY_MESSAGE_SUBTITLE);
}

int GetMessageIconResourceId(const EntityInstance& entity) {
  switch (entity.type().name()) {
    case EntityTypeName::kDriversLicense:
      return IDR_ANDROID_AUTOFILL_ID_CARD;
    case EntityTypeName::kKnownTravelerNumber:
      return IDR_ANDROID_AUTOFILL_FLIGHT;
    case EntityTypeName::kNationalIdCard:
      return IDR_ANDROID_AUTOFILL_ID_CARD;
    case EntityTypeName::kPassport:
      return IDR_ANDROID_AUTOFILL_ID_CARD;
    case EntityTypeName::kRedressNumber:
      return IDR_ANDROID_AUTOFILL_FLIGHT;
    case EntityTypeName::kVehicle:
      return IDR_ANDROID_AUTOFILL_VEHICLE;
    case EntityTypeName::kFlightReservation:
      NOTREACHED() << "Entity is read only and doesn't support save prompts.";
  }
  NOTREACHED();
}

}  // namespace

AutofillAiSaveUpdateEntityFlowManager::AutofillAiSaveUpdateEntityFlowManager(
    content::WebContents* web_contents,
    AutofillMessageController* autofill_message_controller)
    : web_contents_(web_contents),
      autofill_message_controller_(CHECK_DEREF(autofill_message_controller)) {}

AutofillAiSaveUpdateEntityFlowManager::
    ~AutofillAiSaveUpdateEntityFlowManager() = default;

void AutofillAiSaveUpdateEntityFlowManager::OfferSave(
    const EntityInstance& entity,
    std::optional<EntityInstance> old_entity,
    AutofillClient::EntityImportPromptResultCallback prompt_closed_callback) {
  prompt_closed_callback_ = std::move(prompt_closed_callback);
  autofill_message_controller_->Show(
      CreateMessageModel(entity, /*is_save_prompt=*/!old_entity.has_value()));
}

std::unique_ptr<AutofillMessageModel>
AutofillAiSaveUpdateEntityFlowManager::CreateMessageModel(
    const EntityInstance& entity,
    bool is_save_prompt) {
  // Binding with base::Unretained(this) is safe here because
  // AutofillAiSaveUpdateEntityMessageController owns message_. Callbacks won't
  // be called after the current object is destroyed.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_UPDATE_ENTITY);

  message->SetTitle(GetPromptTitle(entity.type().name(), is_save_prompt));
  message->SetDescription(GetMessageDescription());
  message->SetDescriptionMaxLines(kDescriptionMaxLines);
  message->SetPrimaryButtonText(GetPrimaryButtonText(is_save_prompt));
  message->SetPrimaryButtonTextMaxLines(1);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(GetMessageIconResourceId(entity)));

  return std::make_unique<AutofillMessageModel>(
      std::move(message), AutofillMessageModel::Type::kEntitySaveUpdateFlow,
      base::BindOnce(
          &AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction,
          weak_ptr_factory_.GetWeakPtr(), entity),
      base::BindOnce(&AutofillAiSaveUpdateEntityFlowManager::OnMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction(
    const EntityInstance& entity) {
  auto prompt_view_android =
      std::make_unique<AutofillAiSaveUpdateEntityPromptViewAndroid>(
          web_contents_);
  save_update_entity_prompt_controller_ =
      std::make_unique<AutofillAiSaveUpdateEntityPromptController>(
          std::move(prompt_view_android), entity.type().name(),
          std::move(prompt_closed_callback_));
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
      RunPromptClosedCallback(
          AutofillClient::AutofillAiBubbleClosedReason::kClosed);
      break;
    default:
      // Dismissal for any other reason like timeout, tab switch, etc.
      RunPromptClosedCallback(
          AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted);
      break;
  }
}

void AutofillAiSaveUpdateEntityFlowManager::RunPromptClosedCallback(
    AutofillClient::AutofillAiBubbleClosedReason decision) {
  if (prompt_closed_callback_) {
    std::move(prompt_closed_callback_).Run(decision);
  }
}

}  // namespace autofill
