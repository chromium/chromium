// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_

namespace ash::assistant {

// Class that provides the bridge between the ChromeOS UI thread and the
// Libassistant audio input class.
class AudioInputHost {
 public:
  virtual ~AudioInputHost() = default;

  // Called when the mic state associated with the interaction is changed.
  virtual void SetMicState(bool mic_open) = 0;

  // Called when hotword enabled status changed.
  virtual void OnHotwordEnabled(bool enable) = 0;

  virtual void OnConversationTurnStarted() = 0;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
