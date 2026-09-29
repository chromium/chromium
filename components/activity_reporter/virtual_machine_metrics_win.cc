// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/activity_reporter/virtual_machine_win.h"

namespace activity_reporter {

BASE_FEATURE(kVmDetectionExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

VmDetectionResult DetectVirtualMachine() {
  return VmDetectionResult{
      .hypervisor_type = GetHypervisorType(),
      .firmware_vm_vendor = GetFirmwareVmVendor(),
      .cpuid_says_vm = IsRunningInVirtualMachineCpuid(),
      .smbios_says_vm = IsRunningInVirtualMachineSmbios(),
  };
}

VmDetectionAgreement ComputeAgreement(bool cpuid_says_vm, bool smbios_says_vm) {
  if (cpuid_says_vm && smbios_says_vm) {
    return VmDetectionAgreement::kBoth;
  }
  if (cpuid_says_vm) {
    return VmDetectionAgreement::kCpuidOnly;
  }
  if (smbios_says_vm) {
    return VmDetectionAgreement::kSmbiosOnly;
  }
  return VmDetectionAgreement::kNeither;
}

void RecordVirtualMachineHistograms(const VmDetectionResult& result) {
  base::UmaHistogramEnumeration("Windows.VmDetection.HypervisorType",
                                result.hypervisor_type);
  base::UmaHistogramEnumeration("Windows.VmDetection.FirmwareVmVendor",
                                result.firmware_vm_vendor);
  base::UmaHistogramEnumeration(
      "Windows.VmDetection.Agreement",
      ComputeAgreement(result.cpuid_says_vm, result.smbios_says_vm));
}

// TODO(crbug.com/563548288): remove the feature condition once the VM detection
// code lands. The conditional is present so that manual testing is possible on
// branches while avoiding collecting unreliable UMA data.
void RecordVirtualMachineHistogramsAsync() {
  if (!base::FeatureList::IsEnabled(kVmDetectionExperiment)) {
    return;
  }

  // Detection and recording both run on the pool thread; UMA is thread-safe,
  // so no reply is needed. Dropping the sample at shutdown is harmless.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [] { RecordVirtualMachineHistograms(DetectVirtualMachine()); }));
}

}  // namespace activity_reporter
