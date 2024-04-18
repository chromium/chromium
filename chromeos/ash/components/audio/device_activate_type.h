// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_DEVICE_ACTIVATE_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_DEVICE_ACTIVATE_TYPE_H_

namespace ash {

// Indicates the source that activates an audio device.
enum class DeviceActivateType {
  kActivateByPriority = 0,
  kActivateByUser = 1,
  kActivateByRestorePreviousState = 2,
  kActivateByCamera = 3,
  kMaxValue = kActivateByCamera,
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_DEVICE_ACTIVATE_TYPE_H_
