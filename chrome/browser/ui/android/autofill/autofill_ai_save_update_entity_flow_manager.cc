// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_flow_manager.h"

#include <string>

#include "base/check_deref.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/messages/android/message_enums.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::u16string GetMessageDescription() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_SUBTITLE);
}

std::u16string GetMessagePrimaryButtonText() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON);
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
    AutofillMessageController* autofill_message_controller)
    : autofill_message_controller_(CHECK_DEREF(autofill_message_controller)) {}

AutofillAiSaveUpdateEntityFlowManager::
    ~AutofillAiSaveUpdateEntityFlowManager() = default;

void AutofillAiSaveUpdateEntityFlowManager::OfferSave(
    const EntityInstance& entity) {
  autofill_message_controller_->Show(CreateMessageModel(entity));
}

std::unique_ptr<AutofillMessageModel>
AutofillAiSaveUpdateEntityFlowManager::CreateMessageModel(
    const EntityInstance& entity) {
  // Binding with base::Unretained(this) is safe here because
  // AutofillAiSaveUpdateEntityMessageController owns message_. Callbacks won't
  // be called after the current object is destroyed.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_UPDATE_ENTITY);

  message->SetTitle(
      GetPromptTitle(entity.type().name(), /*is_save_prompt=*/true));
  message->SetDescription(GetMessageDescription());
  message->SetDescriptionMaxLines(kDescriptionMaxLines);
  message->SetPrimaryButtonText(GetMessagePrimaryButtonText());
  message->SetPrimaryButtonTextMaxLines(1);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(GetMessageIconResourceId(entity)));

  return std::make_unique<AutofillMessageModel>(
      std::move(message), AutofillMessageModel::Type::kEntitySaveUpdateFlow,
      base::BindOnce(
          &AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&AutofillAiSaveUpdateEntityFlowManager::OnMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutofillAiSaveUpdateEntityFlowManager::OnMessagePrimaryAction() {}

void AutofillAiSaveUpdateEntityFlowManager::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {}

}  // namespace autofill
