// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"

#include "base/logging.h"
#include "components/autofill_assistant/browser/metrics.h"

namespace autofill_assistant {
namespace {

constexpr char kGoogleTtsEngineId[] = "com.google.android.tts";

}  // namespace

AutofillAssistantTtsController::AutofillAssistantTtsController(
    content::TtsController* tts_controller)
    : tts_controller_(tts_controller) {}

AutofillAssistantTtsController::~AutofillAssistantTtsController() {
  // Remove self from receiving any further Tts Events for any pending
  // utterance. Also stops any ongoing/queued utterance with this delegate.
  if (tts_controller_ != nullptr) {
    tts_controller_->RemoveUtteranceEventDelegate(this);
  }
}

void AutofillAssistantTtsController::Speak(const std::string& message,
                                           const std::string& locale) {
  std::unique_ptr<content::TtsUtterance> tts_utterance =
      content::TtsUtterance::Create();
  tts_utterance->SetText(message);
  tts_utterance->SetLang(locale);
  tts_utterance->SetShouldClearQueue(true);
  tts_utterance->SetEventDelegate(this);
  // TtsController will use the default TTS engine if the Google TTS engine
  // is not available.
  tts_utterance->SetEngineId(kGoogleTtsEngineId);

  tts_controller_->SpeakOrEnqueue(std::move(tts_utterance));
}

void AutofillAssistantTtsController::Stop() {
  tts_controller_->Stop();
}

void AutofillAssistantTtsController::SetTtsEventDelegate(
    base::WeakPtr<TtsEventDelegate> tts_event_delegate) {
  // Ensure that it is set only once
  DCHECK(!tts_event_delegate_);

  tts_event_delegate_ = tts_event_delegate;
}

void AutofillAssistantTtsController::OnTtsEvent(
    content::TtsUtterance* utterance,
    content::TtsEventType event_type,
    int char_index,
    int char_length,
    const std::string& error_message) {
  if (!tts_event_delegate_) {
    VLOG(1) << "AssistantAutofillTtsController: No TtsEventDelegate set.";
    return;
  }
  switch (event_type) {
    case content::TTS_EVENT_START:
      tts_event_delegate_->OnTtsEvent(TTS_START);
      Metrics::RecordTtsEngineEvent(Metrics::TtsEngineEvent::TTS_EVENT_START);
      break;
    case content::TTS_EVENT_END:
      tts_event_delegate_->OnTtsEvent(TTS_END);
      Metrics::RecordTtsEngineEvent(Metrics::TtsEngineEvent::TTS_EVENT_END);
      break;
    case content::TTS_EVENT_ERROR:
      VLOG(1) << __func__ << ": " << error_message;
      tts_event_delegate_->OnTtsEvent(TTS_ERROR);
      Metrics::RecordTtsEngineEvent(Metrics::TtsEngineEvent::TTS_EVENT_ERROR);
      break;
    case content::TTS_EVENT_INTERRUPTED:
    case content::TTS_EVENT_CANCELLED:
    case content::TTS_EVENT_WORD:
    case content::TTS_EVENT_SENTENCE:
    case content::TTS_EVENT_MARKER:
    case content::TTS_EVENT_PAUSE:
    case content::TTS_EVENT_RESUME:
      // Do not care about these events. Android does not send back these
      // events anyways.
      Metrics::RecordTtsEngineEvent(Metrics::TtsEngineEvent::TTS_EVENT_OTHER);
      break;
  }
}

}  // namespace autofill_assistant