// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_DEVICEMODE_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_DEVICEMODE_H_

#include "base/component_export.h"

namespace chromeos {

// Returns true when running as system compositor. Ie. using libudev, kms and
// evdev for input and output.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
bool IsRunningAsSystemCompositor();

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_DEVICEMODE_H_
