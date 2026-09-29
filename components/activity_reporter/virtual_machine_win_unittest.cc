// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/virtual_machine_win.h"

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace activity_reporter {

// The detectors are stubs, so these tests cover the metrics plumbing only.

TEST(VirtualMachineMetricsTest, ComputeAgreement) {
  EXPECT_EQ(ComputeAgreement(/*cpuid_says_vm=*/false, /*smbios_says_vm=*/false),
            VmDetectionAgreement::kNeither);
  EXPECT_EQ(ComputeAgreement(/*cpuid_says_vm=*/true, /*smbios_says_vm=*/false),
            VmDetectionAgreement::kCpuidOnly);
  EXPECT_EQ(ComputeAgreement(/*cpuid_says_vm=*/false, /*smbios_says_vm=*/true),
            VmDetectionAgreement::kSmbiosOnly);
  EXPECT_EQ(ComputeAgreement(/*cpuid_says_vm=*/true, /*smbios_says_vm=*/true),
            VmDetectionAgreement::kBoth);
}

// Uses synthetic results so that expectations don't depend on the bot.
TEST(VirtualMachineMetricsTest, RecordsAllHistograms) {
  const struct {
    VmDetectionResult result;
    VmDetectionAgreement expected_agreement;
  } kTestCases[] = {
      {{.hypervisor_type = HypervisorType::kNone,
        .firmware_vm_vendor = FirmwareVmVendor::kNone,
        .cpuid_says_vm = false,
        .smbios_says_vm = false},
       VmDetectionAgreement::kNeither},
      // A physical machine with VBS enabled: CPUID sees the Hyper-V root
      // partition, the firmware still reports the real OEM.
      {{.hypervisor_type = HypervisorType::kHyperVRoot,
        .firmware_vm_vendor = FirmwareVmVendor::kNone,
        .cpuid_says_vm = false,
        .smbios_says_vm = false},
       VmDetectionAgreement::kNeither},
      {{.hypervisor_type = HypervisorType::kVMware,
        .firmware_vm_vendor = FirmwareVmVendor::kNone,
        .cpuid_says_vm = true,
        .smbios_says_vm = false},
       VmDetectionAgreement::kCpuidOnly},
      // Windows on ARM64 in a VM: CPUID cannot run, the firmware still knows.
      {{.hypervisor_type = HypervisorType::kUnsupportedArchitecture,
        .firmware_vm_vendor = FirmwareVmVendor::kHyperV,
        .cpuid_says_vm = false,
        .smbios_says_vm = true},
       VmDetectionAgreement::kSmbiosOnly},
      {{.hypervisor_type = HypervisorType::kKvm,
        .firmware_vm_vendor = FirmwareVmVendor::kGoogleComputeEngine,
        .cpuid_says_vm = true,
        .smbios_says_vm = true},
       VmDetectionAgreement::kBoth},
      // The firmware tables could not be read: "don't know", not "VM".
      {{.hypervisor_type = HypervisorType::kNone,
        .firmware_vm_vendor = FirmwareVmVendor::kReadFailed,
        .cpuid_says_vm = false,
        .smbios_says_vm = false},
       VmDetectionAgreement::kNeither},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;

    RecordVirtualMachineHistograms(test_case.result);

    histogram_tester.ExpectUniqueSample("Windows.VmDetection.HypervisorType",
                                        test_case.result.hypervisor_type, 1);
    histogram_tester.ExpectUniqueSample("Windows.VmDetection.FirmwareVmVendor",
                                        test_case.result.firmware_vm_vendor, 1);
    histogram_tester.ExpectUniqueSample("Windows.VmDetection.Agreement",
                                        test_case.expected_agreement, 1);
  }
}

// Results depend on the machine, so only the counts are checked.
TEST(VirtualMachineMetricsTest, AsyncRecordsWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kVmDetectionExperiment);
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  RecordVirtualMachineHistogramsAsync();
  // Nothing posts back to this thread, so flush the pool to wait for the task.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectTotalCount("Windows.VmDetection.HypervisorType", 1);
  histogram_tester.ExpectTotalCount("Windows.VmDetection.FirmwareVmVendor", 1);
  histogram_tester.ExpectTotalCount("Windows.VmDetection.Agreement", 1);
}

TEST(VirtualMachineMetricsTest, AsyncRecordsNothingWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kVmDetectionExperiment);
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  RecordVirtualMachineHistogramsAsync();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectTotalCount("Windows.VmDetection.HypervisorType", 0);
  histogram_tester.ExpectTotalCount("Windows.VmDetection.FirmwareVmVendor", 0);
  histogram_tester.ExpectTotalCount("Windows.VmDetection.Agreement", 0);
}

// TODO(crbug.com/563548288): delete along with virtual_machine_stub_win.cc.
TEST(VirtualMachineMetricsTest, StubsReportPhysicalMachine) {
  EXPECT_EQ(GetHypervisorType(), HypervisorType::kNone);
  EXPECT_EQ(GetFirmwareVmVendor(), FirmwareVmVendor::kNone);
  EXPECT_FALSE(IsRunningInVirtualMachineCpuid());
  EXPECT_FALSE(IsRunningInVirtualMachineSmbios());
}

}  // namespace activity_reporter
