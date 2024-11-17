// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_SPEECH_RECOGNIZER_FSM_H_
#define COMPONENTS_SPEECH_SPEECH_RECOGNIZER_FSM_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "components/speech/audio_buffer.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"

namespace speech {

// Interface for the speech recognizer finite-state machine used to power the
// Web Speech API.
// TODO(crbug.com/40286514): Remove the scoped_refptr usage.
class SpeechRecognizerFsm {
 public:
  // The Finite State Machine states of the recognizer in sequential order.
  enum FSMState {
    STATE_IDLE = 0,
    STATE_PREPARING,
    STATE_STARTING,
    STATE_ESTIMATING_ENVIRONMENT,
    STATE_WAITING_FOR_SPEECH,
    STATE_RECOGNIZING,
    STATE_WAITING_FINAL_RESULT,
    STATE_ENDED,
    STATE_MAX_VALUE = STATE_ENDED
  };

  // The Finite State Machine events used by the recognizer. Event dispatching
  // must be sequential, otherwise it will break the rules and the assumptions
  // of the finite state automata model.
  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_PREPARE,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_AUDIO_DATA,
    EVENT_ENGINE_RESULT,
    EVENT_ENGINE_ERROR,
    EVENT_AUDIO_ERROR,
    EVENT_MAX_VALUE = EVENT_AUDIO_ERROR
  };

  struct FSMEventArgs {
    explicit FSMEventArgs(FSMEvent event_value);
    FSMEventArgs(const FSMEventArgs& other);
    ~FSMEventArgs();

    FSMEvent event;
    media::mojom::AudioDataS16Ptr audio_data;
    scoped_refptr<AudioChunk> audio_chunk;
    std::vector<media::mojom::WebSpeechRecognitionResultPtr> engine_results;
    media::mojom::SpeechRecognitionError engine_error;
  };

  // Defines the behavior of the recognizer FSM, selecting the appropriate
  // transition according to the current state and event.
  FSMState ExecuteTransitionAndGetNextState(const FSMEventArgs& args);

  // Entry point for pushing any new external event into the recognizer FSM.
  virtual void DispatchEvent(const FSMEventArgs& event_args) = 0;

  // The methods below handle transitions of the recognizer FSM.
  virtual void ProcessAudioPipeline(const FSMEventArgs& event_args) = 0;
  virtual FSMState PrepareRecognition(const FSMEventArgs&) = 0;
  virtual FSMState StartRecording(const FSMEventArgs& event_args) = 0;
  virtual FSMState StartRecognitionEngine(const FSMEventArgs& event_args) = 0;
  virtual FSMState WaitEnvironmentEstimationCompletion(
      const FSMEventArgs& event_args) = 0;
  virtual FSMState DetectUserSpeechOrTimeout(
      const FSMEventArgs& event_args) = 0;
  virtual FSMState StopCaptureAndWaitForResult(
      const FSMEventArgs& event_args) = 0;
  virtual FSMState ProcessIntermediateResult(
      const FSMEventArgs& event_args) = 0;
  virtual FSMState ProcessFinalResult(const FSMEventArgs& event_args) = 0;
  virtual FSMState AbortSilently(const FSMEventArgs& event_args) = 0;
  virtual FSMState AbortWithError(const FSMEventArgs& event_args) = 0;
  virtual FSMState Abort(const media::mojom::SpeechRecognitionError& error) = 0;
  virtual FSMState DetectEndOfSpeech(const FSMEventArgs& event_args) = 0;
  virtual FSMState DoNothing(const FSMEventArgs& event_args) const = 0;
  virtual FSMState NotFeasible(const FSMEventArgs& event_args) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SpeechRecognizerFsm>;
  virtual ~SpeechRecognizerFsm() = default;
  FSMState state_ = STATE_IDLE;
  bool is_dispatching_event_ = false;
};

}  // namespace speech

#endif  // COMPONENTS_SPEECH_SPEECH_RECOGNIZER_FSM_H_
