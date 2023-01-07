// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"

namespace autofill_assistant {

// Used to expose Text-to-Speech functionality in Autofill Assistant.
class AutofillAssistantTtsController : public content::UtteranceEventDelegate {
 public:
  enum TtsEventType {
    TTS_START = 0,  // TTS started playing by TTS Engine
    TTS_END = 1,    // TTS ended playing (does not include interrupted cases)
    TTS_ERROR = 2   // Failed to Play TTS
  };

  class TtsEventDelegate {
   public:
    virtual ~TtsEventDelegate() = default;

    virtual void OnTtsEvent(TtsEventType event) = 0;
  };

  AutofillAssistantTtsController(content::TtsController* tts_controller);

  ~AutofillAssistantTtsController() override;

  // Speaks the message in the given locale. Stops any ongoing TTS.
  //
  // Locale string must be in BCP 47 format, eg: "en-US", "hi-IN".
  //
  // Note: Will trigger a TTS_START event once the engine starts playing the
  // TTS, which will be forwarded to the TtsEventDelegate.
  virtual void Speak(const std::string& message, const std::string& locale);

  // Stops any ongoing TTS.
  //
  // Note: Explicitly stopping a TTS message via this function does not
  // generate any TTS event.
  virtual void Stop();

  void SetTtsEventDelegate(base::WeakPtr<TtsEventDelegate> tts_event_delegate);

  // Overrides UtteranceEventDelegate
  // Note: We will get this callback only for the events related to any current
  // utterance started by this Controller only.
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override;

 private:
  raw_ptr<content::TtsController> tts_controller_ = nullptr;

  base::WeakPtr<TtsEventDelegate> tts_event_delegate_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_TTS_CONTROLLER_H_
