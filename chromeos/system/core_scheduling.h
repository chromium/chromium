// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_CORE_SCHEDULING_H_
#define CHROMEOS_SYSTEM_CORE_SCHEDULING_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace system {

// EnableCoreScheduVlingIfAvailable will turn on core scheduling for a process
// if it's available,
void CHROMEOS_EXPORT EnableCoreSchedulingIfAvailable();

// Returns true if core scheduling is supported in the kernel, and CPU has MDS
// or L1TF vulnerabilities. Core scheduling does not run on CPUs that are not
// vulnerable.
bool CHROMEOS_EXPORT IsCoreSchedulingAvailable();

// Returns number of physical cores. This is useful when deciding the number of
// crosvm vCPUs for devices with per-vCPU core scheduling. When the device uses
// per-VM core scheduling, use base::SysInfo::NumberOfProcessors() instead.
int CHROMEOS_EXPORT NumberOfPhysicalCores();

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_CORE_SCHEDULING_H_
