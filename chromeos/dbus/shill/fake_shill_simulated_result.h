// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace chromeos {

enum class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillSimulatedResult {
  kSuccess,
  kFailure,
  kTimeout
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_SIMULATED_RESULT_H_
