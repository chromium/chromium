// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/speech_recognizer_fsm.h"

#include "base/notreached.h"
#include "components/speech/audio_buffer.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"

namespace speech {

SpeechRecognizerFsm::FSMEventArgs::FSMEventArgs(FSMEvent event_value)
    : event(event_value),
      engine_error(media::mojom::SpeechRecognitionErrorCode::kNone,
                   media::mojom::SpeechAudioErrorDetails::kNone) {}

SpeechRecognizerFsm::FSMEventArgs::FSMEventArgs(const FSMEventArgs& other)
    : event(other.event),
      audio_data(other.audio_data ? other.audio_data->Clone() : nullptr),
      audio_chunk(other.audio_chunk),
      engine_error(other.engine_error) {
  engine_results = mojo::Clone(other.engine_results);
}

SpeechRecognizerFsm::FSMEventArgs::~FSMEventArgs() = default;

SpeechRecognizerFsm::FSMState
SpeechRecognizerFsm::ExecuteTransitionAndGetNextState(
    const FSMEventArgs& event_args) {
  const FSMEvent event = event_args.event;
  switch (state_) {
    case STATE_IDLE:
      switch (event) {
        case EVENT_ABORT:
          return AbortSilently(event_args);
        case EVENT_PREPARE:
          return PrepareRecognition(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return AbortSilently(event_args);
        case EVENT_AUDIO_DATA:     // Corner cases related to queued messages
        case EVENT_ENGINE_RESULT:  // being lately dispatched.
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return DoNothing(event_args);
      }
      break;
    case STATE_PREPARING:
      switch (event) {
        case EVENT_ABORT:
          return AbortSilently(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return StartRecording(event_args);
        case EVENT_STOP_CAPTURE:
          return AbortSilently(event_args);
        case EVENT_AUDIO_DATA:     // Corner cases related to queued messages
        case EVENT_ENGINE_RESULT:  // being lately dispatched.
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return DoNothing(event_args);
      }
      break;
    case STATE_STARTING:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return AbortSilently(event_args);
        case EVENT_AUDIO_DATA:
          return StartRecognitionEngine(event_args);
        case EVENT_ENGINE_RESULT:
          if (event_args.audio_data) {
            return ProcessIntermediateResult(event_args);
          }
          return NotFeasible(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_ESTIMATING_ENVIRONMENT:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return WaitEnvironmentEstimationCompletion(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_WAITING_FOR_SPEECH:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectUserSpeechOrTimeout(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_RECOGNIZING:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
          return StopCaptureAndWaitForResult(event_args);
        case EVENT_AUDIO_DATA:
          return DetectEndOfSpeech(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessIntermediateResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;
    case STATE_WAITING_FINAL_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return AbortWithError(event_args);
        case EVENT_PREPARE:
          return NotFeasible(event_args);
        case EVENT_START:
          return NotFeasible(event_args);
        case EVENT_STOP_CAPTURE:
        case EVENT_AUDIO_DATA:
          return DoNothing(event_args);
        case EVENT_ENGINE_RESULT:
          return ProcessFinalResult(event_args);
        case EVENT_ENGINE_ERROR:
        case EVENT_AUDIO_ERROR:
          return AbortWithError(event_args);
      }
      break;

    case STATE_ENDED:
      return DoNothing(event_args);
  }

  NOTREACHED();
}

}  // namespace speech
