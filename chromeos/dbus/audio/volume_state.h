// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AUDIO_VOLUME_STATE_H_
#define CHROMEOS_DBUS_AUDIO_VOLUME_STATE_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"

namespace chromeos {

struct COMPONENT_EXPORT(DBUS_AUDIO) VolumeState {
  int32_t output_volume;
  bool output_system_mute;
  int32_t input_gain;
  bool input_mute;
  bool output_user_mute;

  VolumeState();
  std::string ToString() const;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AUDIO_VOLUME_STATE_H_
