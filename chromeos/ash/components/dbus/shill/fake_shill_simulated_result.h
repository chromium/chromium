// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_

#include "base/component_export.h"

namespace ash {

enum class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillSimulatedResult {
  kSuccess,
  kFailure,
  kTimeout,
  kInProgress
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_
