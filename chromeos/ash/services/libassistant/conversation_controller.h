// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/conversation_observer.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/mojom/authentication_state_observer.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "chromeos/assistant/internal/action/assistant_action_observer.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos::assistant::action {
class CrosActionModule;
}

namespace ash {

class AssistantClient;

namespace libassistant {

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) ConversationController
    : public mojom::ConversationController,
      public AssistantClientObserver,
      public chromeos::assistant::action::AssistantActionObserver,
      public assistant::ConversationObserver {
 public:
  using AssistantNotification = assistant::AssistantNotification;
  using AssistantQuerySource = assistant::AssistantQuerySource;
  using AssistantFeedback = assistant::AssistantFeedback;

  ConversationController();
  ConversationController(const ConversationController&) = delete;
  ConversationController& operator=(const ConversationController&) = delete;
  ~ConversationController() override;

  void Bind(
      mojo::PendingReceiver<mojom::ConversationController> receiver,
      mojo::PendingRemote<mojom::NotificationDelegate> notification_delegate);

  void AddActionObserver(
      chromeos::assistant::action::AssistantActionObserver* observer);
  void AddAuthenticationStateObserver(
      mojo::PendingRemote<mojom::AuthenticationStateObserver> observer);

  // AssistantClientObserver:
  void OnAssistantClientRunning(AssistantClient* assistant_client) override;
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;

  // mojom::ConversationController implementation:
  void SendTextQuery(const std::string& query,
                     AssistantQuerySource source,
                     bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void RetrieveNotification(AssistantNotification notification,
                            int32_t action_index) override;
  void DismissNotification(AssistantNotification notification) override;
  void SendAssistantFeedback(const AssistantFeedback& feedback) override;
  void AddRemoteObserver(
      mojo::PendingRemote<mojom::ConversationObserver> observer) override;

  // chromeos::assistant::action::AssistantActionObserver:
  void OnShowHtml(const std::string& html_content,
                  const std::string& fallback) override;
  void OnShowText(const std::string& text) override;
  void OnShowSuggestions(
      const std::vector<chromeos::assistant::action::Suggestion>& suggestions)
      override;
  void OnOpenUrl(const std::string& url, bool in_background) override;
  void OnOpenAndroidApp(
      const assistant::AndroidAppInfo& app_info,
      const chromeos::assistant::InteractionInfo& interaction) override;
  void OnScheduleWait(int id, int time_ms) override;
  void OnShowNotification(
      const chromeos::assistant::action::Notification& notification) override;

  // assistant::ConversationObserver:
  void OnInteractionStarted(
      const assistant::AssistantInteractionMetadata& metadata) override;
  void OnInteractionFinished(
      assistant::AssistantInteractionResolution resolution) override;

  const mojo::RemoteSet<mojom::ConversationObserver>* conversation_observers() {
    return &observers_;
  }

  chromeos::assistant::action::CrosActionModule* action_module() {
    return action_module_.get();
  }

  void OnGrpcMessageForTesting(
      const ::assistant::api::OnDeviceStateEventRequest& request);

 private:
  class GrpcEventsObserver;

  void MaybeStopPreviousInteraction();

  mojo::Receiver<mojom::ConversationController> receiver_;
  mojo::RemoteSet<mojom::ConversationObserver> observers_;
  mojo::RemoteSet<mojom::AuthenticationStateObserver>
      authentication_state_observers_;
  mojo::Remote<mojom::NotificationDelegate> notification_delegate_;

  // Owned by ServiceController.
  // Set in `OnAssistantClientCreated()` and unset in
  // `OnDestroyingAssistantClient()`.
  raw_ptr<AssistantClient> assistant_client_ = nullptr;

  // False until libassistant is running for the first time.
  // Any request that comes in before that is an error and will be DCHECK'ed.
  bool requests_are_allowed_ = false;

  std::unique_ptr<GrpcEventsObserver> events_observer_;
  std::unique_ptr<chromeos::assistant::action::CrosActionModule> action_module_;

  std::unique_ptr<base::CancelableOnceClosure> stop_interaction_closure_;
  base::TimeDelta stop_interaction_delay_ = base::Milliseconds(500);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<ConversationController> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_CONTROLLER_H_
