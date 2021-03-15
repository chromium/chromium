// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chromeos/assistant/internal/action/assistant_action_observer.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/libassistant/public/mojom/authentication_state_observer.mojom.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

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
      public AssistantManagerObserver,
      public ::chromeos::assistant::action::AssistantActionObserver {
 public:
  using AssistantNotification = ::chromeos::assistant::AssistantNotification;
  using AssistantQuerySource = ::chromeos::assistant::AssistantQuerySource;
  using AssistantFeedback = ::chromeos::assistant::AssistantFeedback;

  explicit ConversationController(ServiceController* service_controller);
  ConversationController(const ConversationController&) = delete;
  ConversationController& operator=(const ConversationController&) = delete;
  ~ConversationController() override;

  void Bind(
      mojo::PendingReceiver<mojom::ConversationController> receiver,
      mojo::PendingRemote<mojom::NotificationDelegate> notification_delegate);

  void AddActionObserver(
      chromeos::assistant::action::AssistantActionObserver* observer);
  void AddAuthenticationStateObserver(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::AuthenticationStateObserver> observer);

  // AssistantManagerObserver:
  void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

  // mojom::ConversationController implementation:
  void SendTextQuery(const std::string& query,
                     AssistantQuerySource source,
                     bool allow_tts) override;
  void StartEditReminderInteraction(const std::string& client_id) override;
  void RetrieveNotification(const AssistantNotification& notification,
                            int32_t action_index) override;
  void DismissNotification(const AssistantNotification& notification) override;
  void SendAssistantFeedback(const AssistantFeedback& feedback) override;
  void AddRemoteObserver(
      mojo::PendingRemote<mojom::ConversationObserver> observer) override;

  // chromeos::assistant::action::AssistantActionObserver:
  void OnShowHtml(const std::string& html_content,
                  const std::string& fallback) override;
  void OnShowText(const std::string& text) override;
  void OnShowContextualQueryFallback() override;
  void OnShowSuggestions(
      const std::vector<assistant::action::Suggestion>& suggestions) override;
  void OnOpenUrl(const std::string& url, bool in_background) override;
  void OnOpenAndroidApp(
      const chromeos::assistant::AndroidAppInfo& app_info,
      const chromeos::assistant::InteractionInfo& interaction) override;
  void OnScheduleWait(int id, int time_ms) override;

  const mojo::RemoteSet<mojom::ConversationObserver>* conversation_observers() {
    return &observers_;
  }

 private:
  class AssistantManagerDelegateImpl;

  void SendVoicelessInteraction(const std::string& interaction,
                                const std::string& description,
                                bool is_user_initiated);

  assistant_client::AssistantManagerInternal* assistant_manager_internal();

  mojo::Receiver<mojom::ConversationController> receiver_;
  mojo::RemoteSet<mojom::ConversationObserver> observers_;
  mojo::RemoteSet<mojom::AuthenticationStateObserver>
      authentication_state_observers_;
  mojo::Remote<mojom::NotificationDelegate> notification_delegate_;

  // Owned by |LibassistantService|.
  ServiceController* const service_controller_;

  std::unique_ptr<AssistantManagerDelegateImpl> assistant_manager_delegate_;
  std::unique_ptr<assistant::action::CrosActionModule> action_module_;

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<ConversationController> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
