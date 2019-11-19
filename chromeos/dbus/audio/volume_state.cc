// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/audio/volume_state.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

VolumeState::VolumeState()
    : output_volume(0),
      output_system_mute(false),
      input_gain(0),
      input_mute(false),
      output_user_mute(false) {}

std::string VolumeState::ToString() const {
  std::string result;
  base::StringAppendF(&result, "output_volume = %d ", output_volume);
  base::StringAppendF(&result, "output_system_mute = %s ",
                      output_system_mute ? "true" : "false");
  base::StringAppendF(&result, "input_gain = %d ", input_gain);
  base::StringAppendF(&result, "input_mute = %s ",
                      input_mute ? "true" : "false");
  base::StringAppendF(&result, "output_user_mute = %s ",
                      output_user_mute ? "true" : "false");
  return result;
}

}  // namespace chromeos
