// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_

#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/display_controller.mojom.h"
#include "libassistant/shared/public/conversation_state_listener.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace libassistant {
namespace mojom {
class SpeechRecognitionObserver;
}  // namespace mojom
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class ConversationStateListenerImpl
    : public assistant_client::ConversationStateListener,
      public AssistantManagerObserver {
 public:
  explicit ConversationStateListenerImpl(
      mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
          speech_recognition_observers);
  ConversationStateListenerImpl(const ConversationStateListenerImpl&) = delete;
  ConversationStateListenerImpl& operator=(
      const ConversationStateListenerImpl&) = delete;
  ~ConversationStateListenerImpl() override;

 private:
  // AssistantManagerObserver implementation:
  void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override;

  // assistant_client::ConversationStateListener implementation:
  void OnRecognitionStateChanged(
      RecognitionState state,
      const RecognitionResult& recognition_result) override;

  // Owned by |LibassistantService|.
  mojo::RemoteSet<mojom::SpeechRecognitionObserver>&
      speech_recognition_observers_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_CONVERSATION_STATE_LISTENER_IMPL_H_
