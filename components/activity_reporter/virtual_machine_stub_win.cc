// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/virtual_machine_win.h"

namespace activity_reporter {

// Placeholders that report a physical machine. Do not enable
// `kVmDetectionExperiment` until the real detectors land.
//
// TODO(crbug.com/563548288): replace this file with
// virtual_machine_cpuid_win.cc and virtual_machine_smbios_win.cc.

HypervisorType GetHypervisorType() {
  return HypervisorType::kNone;
}

bool IsRunningInVirtualMachineCpuid() {
  return false;
}

FirmwareVmVendor GetFirmwareVmVendor() {
  return FirmwareVmVendor::kNone;
}

bool IsRunningInVirtualMachineSmbios() {
  return false;
}

}  // namespace activity_reporter
