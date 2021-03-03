// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_AUDIO_INPUT_HOST_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_AUDIO_INPUT_HOST_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {
namespace assistant {

// Class that provides the bridge between the ChromeOS UI thread and the
// Libassistant audio input class.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioInputHost {
 public:
  virtual ~AudioInputHost() = default;

  // Called when the mic state associated with the interaction is changed.
  virtual void SetMicState(bool mic_open) = 0;

  // Called when hotword enabled status changed.
  virtual void OnHotwordEnabled(bool enable) = 0;

  virtual void OnConversationTurnStarted() = 0;
  virtual void OnConversationTurnFinished() = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_AUDIO_INPUT_HOST_H_
