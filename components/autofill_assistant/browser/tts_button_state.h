// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TTS_BUTTON_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TTS_BUTTON_STATE_H_

namespace autofill_assistant {

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.header)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantTtsButtonState
enum TtsButtonState {
  // No TTS is playing.
  DEFAULT = 0,

  // TTS is playing.
  PLAYING = 1,

  // No TTS is playing, autoplay of TTS messages is suppressed.
  DISABLED = 2
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TTS_BUTTON_STATE_H_
