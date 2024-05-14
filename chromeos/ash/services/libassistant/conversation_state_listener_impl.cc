// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/conversation_state_listener_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/libassistant/audio_input_controller.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_observer.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace ash::libassistant {

namespace {

// A macro which ensures we are running on the mojom thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

}  // namespace

ConversationStateListenerImpl::ConversationStateListenerImpl(
    mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
        speech_recognition_observers,
    const mojo::RemoteSet<mojom::ConversationObserver>* conversation_observers,
    AudioInputController* audio_input_controller)
    : speech_recognition_observers_(*speech_recognition_observers),
      conversation_observers_(*conversation_observers),
      audio_input_controller_(audio_input_controller),
      mojom_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(speech_recognition_observers);
  DCHECK(conversation_observers);
  DCHECK(audio_input_controller);
}

ConversationStateListenerImpl::~ConversationStateListenerImpl() = default;

void ConversationStateListenerImpl::OnAssistantClientCreated(
    AssistantClient* assistant_client) {
  assistant_client->assistant_manager()->AddConversationStateListener(this);
}

void ConversationStateListenerImpl::OnRecognitionStateChanged(
    RecognitionState state,
    const RecognitionResult& recognition_result) {
  ENSURE_MOJOM_THREAD(&ConversationStateListenerImpl::OnRecognitionStateChanged,
                      state, recognition_result);

  switch (state) {
    case RecognitionState::STARTED:
      for (auto& observer : *speech_recognition_observers_) {
        observer->OnSpeechRecognitionStart();
      }
      break;
    case RecognitionState::INTERMEDIATE_RESULT:
      for (auto& observer : *speech_recognition_observers_) {
        observer->OnIntermediateResult(recognition_result.high_confidence_text,
                                       recognition_result.low_confidence_text);
      }
      break;
    case RecognitionState::END_OF_UTTERANCE:
      for (auto& observer : *speech_recognition_observers_) {
        observer->OnSpeechRecognitionEnd();
      }
      break;
    case RecognitionState::FINAL_RESULT:
      for (auto& observer : *speech_recognition_observers_) {
        observer->OnFinalResult(recognition_result.recognized_speech);
      }
      break;
  }
}

void ConversationStateListenerImpl::OnConversationTurnFinished(
    assistant_client::ConversationStateListener::Resolution resolution) {
  ENSURE_MOJOM_THREAD(
      &ConversationStateListenerImpl::OnConversationTurnFinished, resolution);

  // TODO(b/179924068): refactor |AudioInputController| to be a normal
  // |mojom::ConversationObserver| once we figured out a better approach to
  // handle those edge cases.
  audio_input_controller_->OnInteractionFinished(resolution);

  switch (resolution) {
    // Interaction ended normally.
    case Resolution::NORMAL:
    case Resolution::NORMAL_WITH_FOLLOW_ON:
    case Resolution::NO_RESPONSE:
      NotifyInteractionFinished(
          assistant::AssistantInteractionResolution::kNormal);
      return;
    // Interaction ended due to interruption.
    case Resolution::BARGE_IN:
    case Resolution::CANCELLED:
      NotifyInteractionFinished(
          assistant::AssistantInteractionResolution::kInterruption);
      return;
    // Interaction ended due to mic timeout.
    case Resolution::TIMEOUT:
      NotifyInteractionFinished(
          assistant::AssistantInteractionResolution::kMicTimeout);
      return;
    // Interaction ended due to error.
    case Resolution::COMMUNICATION_ERROR:
      NotifyInteractionFinished(
          assistant::AssistantInteractionResolution::kError);
      return;
    // Interaction ended because the device was not selected to produce a
    // response. This occurs due to multi-device hotword loss.
    case Resolution::DEVICE_NOT_SELECTED:
      NotifyInteractionFinished(
          assistant::AssistantInteractionResolution::kMultiDeviceHotwordLoss);
      return;
    // This is only applicable in longform barge-in mode, which we do not use.
    case Resolution::LONGFORM_KEEP_MIC_OPEN:
    case Resolution::BLUE_STEEL_ON_DEVICE_REJECTION:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void ConversationStateListenerImpl::OnRespondingStarted(
    bool is_error_response) {
  ENSURE_MOJOM_THREAD(&ConversationStateListenerImpl::OnRespondingStarted,
                      is_error_response);

  for (auto& observer : *conversation_observers_) {
    observer->OnTtsStarted(is_error_response);
  }
}

void ConversationStateListenerImpl::NotifyInteractionFinished(
    assistant::AssistantInteractionResolution resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : *conversation_observers_) {
    observer->OnInteractionFinished(resolution);
  }
}

}  // namespace ash::libassistant
