// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_

#include <string>

#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/conversation_observer.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

// Component supporting conversation related functionalities, includes the
// ability to start an interaction, register action module, etc.
class ConversationControllerProxy {
 public:
  explicit ConversationControllerProxy(
      mojo::PendingRemote<chromeos::libassistant::mojom::ConversationController>
          conversation_controller_remote);
  ConversationControllerProxy(const ConversationControllerProxy&) = delete;
  ConversationControllerProxy& operator=(const ConversationControllerProxy&) =
      delete;
  ~ConversationControllerProxy();

  void AddConversationObserver(
      mojo::PendingRemote<chromeos::libassistant::mojom::ConversationObserver>
          observer);

  chromeos::libassistant::mojom::ConversationController& controller() {
    return *conversation_controller_remote_.get();
  }

  // Starts an interaction to edit the reminder uniquely identified by
  // |client_id|.
  void StartEditReminderInteraction(const std::string& client_id);

  // Retrieves a notification identified by |action_index|.
  void RetrieveNotification(const AssistantNotification& notification,
                            int32_t action_index);

  // Dismisses a notification.
  void DismissNotification(const AssistantNotification& notification);

  // Sends an Assistant feedback to Assistant server.
  void SendAssistantFeedback(const AssistantFeedback& feedback);

 private:
  mojo::Remote<chromeos::libassistant::mojom::ConversationController>
      conversation_controller_remote_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_
