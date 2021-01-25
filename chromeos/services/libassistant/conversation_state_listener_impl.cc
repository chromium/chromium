// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_state_listener_impl.h"
#include "chromeos/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "libassistant/shared/public/assistant_manager.h"

namespace chromeos {
namespace libassistant {

ConversationStateListenerImpl::ConversationStateListenerImpl(
    mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
        speech_recognition_observers)
    : speech_recognition_observers_(*speech_recognition_observers) {
  DCHECK(speech_recognition_observers);
}

ConversationStateListenerImpl::~ConversationStateListenerImpl() = default;

void ConversationStateListenerImpl::OnAssistantManagerCreated(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager->AddConversationStateListener(this);
}

void ConversationStateListenerImpl::OnRecognitionStateChanged(
    RecognitionState state,
    const RecognitionResult& recognition_result) {
  switch (state) {
    case RecognitionState::STARTED:
      for (auto& observer : speech_recognition_observers_)
        observer->OnSpeechRecognitionStart();
      break;
    case RecognitionState::INTERMEDIATE_RESULT:
      for (auto& observer : speech_recognition_observers_) {
        observer->OnIntermediateResult(recognition_result.high_confidence_text,
                                       recognition_result.low_confidence_text);
      }
      break;
    case RecognitionState::END_OF_UTTERANCE:
      for (auto& observer : speech_recognition_observers_)
        observer->OnSpeechRecognitionEnd();
      break;
    case RecognitionState::FINAL_RESULT:
      for (auto& observer : speech_recognition_observers_)
        observer->OnFinalResult(recognition_result.recognized_speech);
      break;
  }
}

}  // namespace libassistant
}  // namespace chromeos
