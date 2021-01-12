// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"

#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/service_controller.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace libassistant {

ConversationController::ConversationController(
    ServiceController* service_controller)
    : receiver_(this), service_controller_(service_controller) {
  DCHECK(service_controller_);
}

ConversationController::~ConversationController() = default;

void ConversationController::Bind(
    mojo::PendingReceiver<mojom::ConversationController> receiver) {
  // Cannot bind the receiver twice.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
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
  std::string interaction =
      chromeos::assistant::CreateTextQueryInteraction(query);

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

assistant_client::AssistantManagerInternal*
ConversationController::assistant_manager_internal() {
  return service_controller_->assistant_manager_internal();
}

}  // namespace libassistant
}  // namespace chromeos
