// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda_speech_recognizer_impl.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/speech/audio_buffer.h"
#include "components/speech/endpointer/endpointer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

SodaSpeechRecognizerImpl::SodaSpeechRecognizerImpl(
    bool continuous,
    int sample_rate,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizer>
        recognizer_remote,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        recognizer_client_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder)
    : endpointer_(sample_rate),
      sample_rate_(sample_rate),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      session_client_(std::move(session_client)),
      speech_recognition_recognizer_(std::move(recognizer_remote)),
      speech_recognition_recognizer_client_(
          this,
          std::move(recognizer_client_receiver)),
      audio_forwarder_(this, std::move(audio_forwarder)) {
  send_audio_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &SodaSpeechRecognizerImpl::SendAudioToSpeechRecognitionService,
      weak_ptr_factory_.GetWeakPtr()));

  if (!continuous) {
    // In single shot (non-continous) recognition,
    // the session is automatically ended after:
    //  - 0.5 seconds of silence if time <  3 seconds
    //  - 1   seconds of silence if time >= 3 seconds
    // These values are arbitrarily defined.
    constexpr float kSpeechCompleteSilenceLength = 0.5f;
    constexpr float kLongSpeechCompleteSilenceLength = 1.0f;
    constexpr float kLongSpeechLength = 3.0f;
    endpointer_.set_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond * kSpeechCompleteSilenceLength);
    endpointer_.set_long_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond * kLongSpeechCompleteSilenceLength);
    endpointer_.set_long_speech_length(base::Time::kMicrosecondsPerSecond *
                                       kLongSpeechLength);
  } else {
    // In continuous recognition, the session is automatically ended after 15
    // seconds of silence.
    constexpr float kSpeechInputCompleteSilenceLength = 15.0f;
    endpointer_.set_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond * kSpeechInputCompleteSilenceLength);
    endpointer_.set_long_speech_length(0);  // Use only a single timeout.
  }
  endpointer_.StartSession();

  StartRecognition();
}

SodaSpeechRecognizerImpl::~SodaSpeechRecognizerImpl() {
  endpointer_.EndSession();
}

// -------  Methods that trigger Finite State Machine (FSM) events ------------

// NOTE: all the external events and requests should be enqueued (PostTask) in
// order to preserve the relationship of causality between events and avoid
// interleaved event processing due to synchronous callbacks.

void SodaSpeechRecognizerImpl::Abort() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_ABORT)));
}

void SodaSpeechRecognizerImpl::StopCapture() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_STOP_CAPTURE)));
}

void SodaSpeechRecognizerImpl::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& recognition_result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  std::move(reply).Run(/*success=*/true);

  waiting_for_final_result_ = !recognition_result.is_final;

  // Map recognition results.
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
  results.push_back(media::mojom::WebSpeechRecognitionResult::New());
  media::mojom::WebSpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = !recognition_result.is_final;

  media::mojom::SpeechRecognitionHypothesisPtr hypothesis =
      media::mojom::SpeechRecognitionHypothesis::New();

  // The Speech On-Device API (SODA) doesn't include a confidence score for the
  // recognition result, so use 1.0f to indicate full confidence.
  constexpr float kSpeechRecognitionConfidence = 1.0f;
  hypothesis->confidence = kSpeechRecognitionConfidence;
  hypothesis->utterance = base::UTF8ToUTF16(recognition_result.transcription);
  result->hypotheses.push_back(std::move(hypothesis));

  FSMEventArgs event_args(EVENT_ENGINE_RESULT);
  event_args.engine_results = mojo::Clone(results);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

void SodaSpeechRecognizerImpl::OnSpeechRecognitionError() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_ENGINE_ERROR)));
}

void SodaSpeechRecognizerImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  // Do nothing. Language identification events are not used by the Web Speech
  // API.
}

void SodaSpeechRecognizerImpl::OnSpeechRecognitionStopped() {
  StopCapture();
}

void SodaSpeechRecognizerImpl::AddAudioFromRenderer(
    media::mojom::AudioDataS16Ptr buffer) {
  FSMEventArgs event_args(EVENT_AUDIO_DATA);
  event_args.audio_data = std::move(buffer);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

void SodaSpeechRecognizerImpl::StartRecognition() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_PREPARE)));
}

void SodaSpeechRecognizerImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr audio_data) {
  DCHECK(audio_data);
  DCHECK(speech_recognition_recognizer_.is_bound());
  speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
      std::move(audio_data));
}

void SodaSpeechRecognizerImpl::DispatchEvent(const FSMEventArgs& event_args) {
  DCHECK_LE(event_args.event, EVENT_MAX_VALUE);
  DCHECK_LE(state_, STATE_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;

  if (event_args.event == EVENT_AUDIO_DATA) {
    DCHECK(event_args.audio_data.get() != nullptr);
    ProcessAudioPipeline(event_args);
  }

  // The audio pipeline must be processed before the event dispatch, otherwise
  // it would take actions according to the future state instead of the current.
  state_ = ExecuteTransitionAndGetNextState(event_args);
  is_dispatching_event_ = false;
}

void SodaSpeechRecognizerImpl::ProcessAudioPipeline(
    const FSMEventArgs& event_args) {
  DCHECK(event_args.audio_data);

  SendAudioToSpeechRecognitionService(event_args.audio_data->Clone());
  num_samples_recorded_ += event_args.audio_data->frame_count;
  if (state_ >= STATE_ESTIMATING_ENVIRONMENT && state_ <= STATE_RECOGNIZING) {
    float rms = 0.0f;
    endpointer_.ProcessAudio(event_args.audio_data->data.data(),
                             event_args.audio_data->frame_count, &rms);
  }
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be not reentrant (themselves and each other);
//  - event_args members are guaranteed to be stable during the call;
//  - The class won't be freed in the meanwhile due to callbacks;
SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::PrepareRecognition(
    const FSMEventArgs&) {
  DCHECK_EQ(state_, STATE_IDLE);
  session_client_->Started();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SodaSpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_START)));

  return STATE_PREPARING;
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::StartRecording(
    const FSMEventArgs&) {
  DCHECK_EQ(state_, STATE_PREPARING);
  num_samples_recorded_ = 0;

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. We wait in the state
  // ESTIMATING_ENVIRONMENT until such interval has elapsed before switching
  // to user input mode.
  endpointer_.SetEnvironmentEstimationMode();

  return STATE_STARTING;
}

SodaSpeechRecognizerImpl::FSMState
SodaSpeechRecognizerImpl::StartRecognitionEngine(
    const FSMEventArgs& event_args) {
  // This is the first audio packet captured, so the recognition engine is
  // started and the delegate notified about the event.
  session_client_->AudioStarted();

  return STATE_ESTIMATING_ENVIRONMENT;
}

SodaSpeechRecognizerImpl::FSMState
SodaSpeechRecognizerImpl::WaitEnvironmentEstimationCompletion(
    const FSMEventArgs&) {
  DCHECK(endpointer_.IsEstimatingEnvironment());

  // Use an arbitrary endpointer estimation time of 3 seconds.
  constexpr base::TimeDelta kEndpointerEstimationTime =
      base::Milliseconds(3000);
  if (GetElapsedTime() >= kEndpointerEstimationTime) {
    endpointer_.SetUserInputMode();
    return STATE_WAITING_FOR_SPEECH;
  }

  return STATE_ESTIMATING_ENVIRONMENT;
}

SodaSpeechRecognizerImpl::FSMState
SodaSpeechRecognizerImpl::DetectUserSpeechOrTimeout(const FSMEventArgs&) {
  if (endpointer_.DidStartReceivingSpeech()) {
    if (!sound_started_) {
      session_client_->SoundStarted();
    }

    return STATE_RECOGNIZING;
  }

  // Use an arbitrary time out duration of 8 seconds.
  constexpr base::TimeDelta kNoSpeechTimeout = base::Milliseconds(8000);
  if (GetElapsedTime() >= kNoSpeechTimeout) {
    return Abort(media::mojom::SpeechRecognitionError(
        media::mojom::SpeechRecognitionErrorCode::kNoSpeech,
        media::mojom::SpeechAudioErrorDetails::kNone));
  }

  return STATE_WAITING_FOR_SPEECH;
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::DetectEndOfSpeech(
    const FSMEventArgs& event_args) {
  if (endpointer_.speech_input_complete()) {
    return StopCaptureAndWaitForResult(event_args);
  }
  return STATE_RECOGNIZING;
}

SodaSpeechRecognizerImpl::FSMState
SodaSpeechRecognizerImpl::StopCaptureAndWaitForResult(const FSMEventArgs&) {
  DCHECK(state_ >= STATE_ESTIMATING_ENVIRONMENT && state_ <= STATE_RECOGNIZING);
  if (state_ > STATE_WAITING_FOR_SPEECH) {
    session_client_->SoundEnded();
    sound_started_ = false;
  }

  session_client_->AudioEnded();

  if (waiting_for_final_result_) {
    return STATE_WAITING_FINAL_RESULT;
  }

  session_client_->Ended();

  return STATE_ENDED;
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::AbortSilently(
    const FSMEventArgs& event_args) {
  DCHECK_NE(event_args.event, EVENT_ENGINE_ERROR);
  return Abort(media::mojom::SpeechRecognitionError(
      media::mojom::SpeechRecognitionErrorCode::kNone,
      media::mojom::SpeechAudioErrorDetails::kNone));
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::AbortWithError(
    const FSMEventArgs& event_args) {
  if (event_args.event == EVENT_ENGINE_ERROR) {
    return Abort(event_args.engine_error);
  }
  return Abort(media::mojom::SpeechRecognitionError(
      media::mojom::SpeechRecognitionErrorCode::kAborted,
      media::mojom::SpeechAudioErrorDetails::kNone));
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::Abort(
    const media::mojom::SpeechRecognitionError& error) {
  if (state_ == STATE_PREPARING) {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  if (state_ > STATE_WAITING_FOR_SPEECH &&
      state_ < STATE_WAITING_FINAL_RESULT) {
    session_client_->SoundEnded();
    sound_started_ = false;
  }

  if (state_ > STATE_STARTING && state_ < STATE_WAITING_FINAL_RESULT) {
    session_client_->AudioEnded();
  }

  if (error.code != media::mojom::SpeechRecognitionErrorCode::kNone) {
    session_client_->ErrorOccurred(
        media::mojom::SpeechRecognitionError::New(error));
  }

  session_client_->Ended();

  return STATE_ENDED;
}

SodaSpeechRecognizerImpl::FSMState
SodaSpeechRecognizerImpl::ProcessIntermediateResult(
    const FSMEventArgs& event_args) {
  // In continuous recognition, intermediate results can occur even when we are
  // in the ESTIMATING_ENVIRONMENT or WAITING_FOR_SPEECH states (if the
  // recognition engine is "faster" than our endpointer). In these cases we
  // skip the endpointer and fast-forward to the RECOGNIZING state, with respect
  // of the events triggering order.
  state_ = STATE_RECOGNIZING;

  if (!sound_started_) {
    sound_started_ = true;
    session_client_->SoundStarted();
  }

  session_client_->ResultRetrieved(mojo::Clone(event_args.engine_results));

  return STATE_RECOGNIZING;
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::ProcessFinalResult(
    const FSMEventArgs& event_args) {
  const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results =
      event_args.engine_results;
  if (base::ranges::any_of(results, [](const auto& result) {
        return !result->hypotheses.empty();
      })) {
    session_client_->ResultRetrieved(mojo::Clone(results));
  }

  session_client_->Ended();
  return STATE_ENDED;
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::DoNothing(
    const FSMEventArgs&) const {
  return state_;  // Just keep the current state.
}

SodaSpeechRecognizerImpl::FSMState SodaSpeechRecognizerImpl::NotFeasible(
    const FSMEventArgs& event_args) {
  NOTREACHED() << "Unfeasible event " << event_args.event << " in state "
               << state_;
}

base::TimeDelta SodaSpeechRecognizerImpl::GetElapsedTime() const {
  return media::AudioTimestampHelper::FramesToTime(num_samples_recorded_,
                                                   sample_rate_);
}

}  // namespace speech
