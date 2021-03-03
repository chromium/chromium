// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace assistant_client {
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
namespace action {
class CrosActionModule;
}  // namespace action
}  // namespace assistant

namespace libassistant {

class ServiceController;

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) ConversationController
    : public mojom::ConversationController,
      public AssistantManagerObserver {
 public:
  using AssistantNotification = ::chromeos::assistant::AssistantNotification;
  using AssistantFeedback = ::chromeos::assistant::AssistantFeedback;

  explicit ConversationController(ServiceController* service_controller);
  ConversationController(const ConversationController&) = delete;
  ConversationController& operator=(const ConversationController&) = delete;
  ~ConversationController() override;

  void Bind(mojo::PendingReceiver<mojom::ConversationController> receiver);

  // AssistantManagerObserver:
  void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

  // mojom::ConversationController implementation:
  void SendTextQuery(
      const std::string& query,
      bool allow_tts,
      const base::Optional<std::string>& conversation_id) override;
  void StartEditReminderInteraction(const std::string& client_id) override;
  void RetrieveNotification(const AssistantNotification& notification,
                            int32_t action_index) override;
  void DismissNotification(const AssistantNotification& notification) override;
  void SendAssistantFeedback(const AssistantFeedback& feedback) override;

 private:
  void SendVoicelessInteraction(const std::string& interaction,
                                const std::string& description,
                                bool is_user_initiated);

  assistant_client::AssistantManagerInternal* assistant_manager_internal();

  mojo::Receiver<mojom::ConversationController> receiver_;

  // Owned by |LibassistantService|.
  ServiceController* const service_controller_;

  std::unique_ptr<assistant::action::CrosActionModule> action_module_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
