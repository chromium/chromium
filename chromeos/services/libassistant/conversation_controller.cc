// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"

#include <memory>

#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/service_controller.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace libassistant {

ConversationController::ConversationController(
    ServiceController* service_controller)
    : receiver_(this),
      service_controller_(service_controller),
      action_module_(std::make_unique<assistant::action::CrosActionModule>(
          assistant::features::IsAppSupportEnabled(),
          assistant::features::IsWaitSchedulingEnabled())) {
  DCHECK(service_controller_);
}

ConversationController::~ConversationController() = default;

void ConversationController::Bind(
    mojo::PendingReceiver<mojom::ConversationController> receiver) {
  // Cannot bind the receiver twice.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

void ConversationController::OnAssistantManagerCreated(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  // Registers ActionModule when AssistantManagerInternal has been created
  // but not yet started.
  assistant_manager_internal->RegisterActionModule(action_module_.get());

  auto* v1_api = assistant::LibassistantV1Api::Get();
  // LibassistantV1Api should be ready to use by this time.
  DCHECK(v1_api);

  v1_api->SetActionModule(action_module_.get());
}

void ConversationController::SendTextQuery(
    const std::string& query,
    bool allow_tts,
    const base::Optional<std::string>& conversation_id) {
  // DCHECKs if this function gets invoked after the service has been fully
  // started.
  // TODO(meilinw): only check for the |ServiceState::kRunning| state instead
  // after it has been wired up.
  DCHECK(service_controller_->IsStarted())
      << "Libassistant service is not ready to handle queries.";
  DCHECK(assistant_manager_internal());

  // Configs |VoicelessOptions|.
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  if (!allow_tts) {
    options.modality =
        assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;
  }
  // Ensure LibAssistant uses the requested conversation id.
  if (conversation_id.has_value())
    options.conversation_turn_id = conversation_id.value();

  // Builds text interaction.
  std::string interaction = assistant::CreateTextQueryInteraction(query);

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

void ConversationController::StartEditReminderInteraction(
    const std::string& client_id) {
  SendVoicelessInteraction(assistant::CreateEditReminderInteraction(client_id),
                           /*description=*/std::string(),
                           /*is_user_initiated=*/true);
}

void ConversationController::RetrieveNotification(
    const AssistantNotification& notification,
    int32_t action_index) {
  const std::string request_interaction =
      assistant::SerializeNotificationRequestInteraction(
          notification.server_id, notification.consistency_token,
          notification.opaque_token, action_index);

  SendVoicelessInteraction(request_interaction,
                           /*description=*/"RequestNotification",
                           /*is_user_initiated=*/true);
}

void ConversationController::DismissNotification(
    const AssistantNotification& notification) {
  // |assistant_manager_internal()| may not exist if we are dismissing
  // notifications as part of a shutdown sequence.
  if (!assistant_manager_internal())
    return;

  const std::string dismissed_interaction =
      assistant::SerializeNotificationDismissedInteraction(
          notification.server_id, notification.consistency_token,
          notification.opaque_token, {notification.grouping_key});

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = notification.obfuscated_gaia_id;

  assistant_manager_internal()->SendVoicelessInteraction(
      dismissed_interaction, /*description=*/"DismissNotification", options,
      [](auto) {});
}

void ConversationController::SendAssistantFeedback(
    const AssistantFeedback& feedback) {
  std::string raw_image_data(feedback.screenshot_png.begin(),
                             feedback.screenshot_png.end());
  const std::string interaction = assistant::CreateSendFeedbackInteraction(
      feedback.assistant_debug_info_allowed, feedback.description,
      raw_image_data);

  SendVoicelessInteraction(interaction,
                           /*description=*/"send feedback with details",
                           /*is_user_initiated=*/false);
}

void ConversationController::SendVoicelessInteraction(
    const std::string& interaction,
    const std::string& description,
    bool is_user_initiated) {
  assistant_client::VoicelessOptions voiceless_options;
  voiceless_options.is_user_initiated = is_user_initiated;

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, description, voiceless_options, [](auto) {});
}

assistant_client::AssistantManagerInternal*
ConversationController::assistant_manager_internal() {
  return service_controller_->assistant_manager_internal();
}

}  // namespace libassistant
}  // namespace chromeos
