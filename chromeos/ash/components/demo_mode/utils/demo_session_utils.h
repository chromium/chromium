// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_

#include "base/component_export.h"

namespace ash::demo_mode {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Whether the device is set up to run demo sessions.
bool IsDeviceInDemoMode();

}  // namespace ash::demo_mode

#endif  // CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
