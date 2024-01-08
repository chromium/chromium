// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::libassistant {

namespace mojom {
class ConversationObserver;
class SpeechRecognitionObserver;
}  // namespace mojom

class AudioInputController;

class ConversationStateListenerImpl
    : public assistant_client::ConversationStateListener,
      public AssistantClientObserver {
 public:
  ConversationStateListenerImpl(
      mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
          speech_recognition_observers,
      const mojo::RemoteSet<mojom::ConversationObserver>*
          conversation_observers,
      AudioInputController* audio_input_controller);
  ConversationStateListenerImpl(const ConversationStateListenerImpl&) = delete;
  ConversationStateListenerImpl& operator=(
      const ConversationStateListenerImpl&) = delete;
  ~ConversationStateListenerImpl() override;

 private:
  // AssistantClientObserver implementation:
  void OnAssistantClientCreated(AssistantClient* assistant_client) override;

  // assistant_client::ConversationStateListener implementation:
  void OnRecognitionStateChanged(
      RecognitionState state,
      const RecognitionResult& recognition_result) override;
  void OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution resolution)
      override;
  void OnRespondingStarted(bool is_error_response) override;

  void NotifyInteractionFinished(
      assistant::AssistantInteractionResolution resolution);

  // Owned by |LibassistantService|.
  const raw_ref<mojo::RemoteSet<mojom::SpeechRecognitionObserver>>
      speech_recognition_observers_;

  // Owned by |ConversationController|.
  const raw_ref<const mojo::RemoteSet<mojom::ConversationObserver>>
      conversation_observers_;

  const raw_ptr<AudioInputController> audio_input_controller_ = nullptr;

  // The callbacks from Libassistant are called on a different sequence,
  // so this sequence checker ensures that no other methods are called on the
  // libassistant sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<ConversationStateListenerImpl> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_
