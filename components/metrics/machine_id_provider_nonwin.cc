// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

#include <stdint.h>

#include "base/check.h"
#include "base/system/sys_info.h"

namespace metrics {

// Checks if hardware model name is available.
bool MachineIdProvider::HasId() {
  return !base::SysInfo::HardwareModelName().empty();
}

// On non-windows, the machine id is based on the hardware model name.
// This will suffice as users are unlikely to change to the same machine model.
std::string MachineIdProvider::GetMachineId() {
  // Gets hardware model name. (e.g. 'Macbook Pro 16,1', 'iPhone 9,3')
  std::string hardware_model_name = base::SysInfo::HardwareModelName();

  // This function should not be called if hardware model name is unavailable.
  DCHECK(!hardware_model_name.empty());

  return hardware_model_name;
}
}  //  namespace metrics
