// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_H_
#define COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_H_

// EXPERIMENTAL. Two implementations of "is this process running in a VM
// guest?" on Windows, compared against each other to measure how often they
// disagree. CPUID is cheap but x86-only and cannot see through a nested Hyper-V
// root partition; SMBIOS works on ARM64 and sees through nesting, but needs a
// syscall and relies on string matching.
//
// TODO(crbug.com/563548288): the detectors are currently stubs
// (virtual_machine_stub_win.cc).
//
// Neither implementation resists a hypervisor that hides itself, so do not use
// them for security or anti-abuse decisions. Only the closed enums below may be
// recorded; raw firmware strings must never be logged or exfiltrated.

#include "base/feature.h"

namespace activity_reporter {

// Gates the experiment. Disabled by default.
BASE_DECLARE_FEATURE(kVmDetectionExperiment);

// The hypervisor identified via the CPUID hypervisor leaves.
//
// These values are persisted to logs. Entries must not be renumbered and
// numeric values must never be reused.
// LINT.IfChange(HypervisorType)
enum class HypervisorType {
  // No hypervisor present: a physical machine.
  kNone = 0,
  // Hypervisor-present bit is set, but the vendor was not recognized.
  kUnknown = 1,
  // Not an x86 CPU (Windows on ARM64), so CPUID could not run. "Don't know",
  // not "physical machine".
  kUnsupportedArchitecture = 2,
  // Hyper-V child partition: Hyper-V/Azure VM, Windows Sandbox, WDAG, WSL2.
  kHyperVGuest = 3,
  // Hyper-V root partition, i.e. the host: a physical machine with VBS/HVCI,
  // Credential Guard, WSL2 or the Hyper-V role enabled.
  kHyperVRoot = 4,
  kVMware = 5,
  kVirtualBox = 6,
  kKvm = 7,
  kQemuTcg = 8,
  kXen = 9,
  kParallels = 10,
  kBhyve = 11,
  kAcrn = 12,
  kMaxValue = kAcrn,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:HypervisorType)

// The virtualization vendor identified from the SMBIOS/firmware strings.
//
// These values are persisted to logs. Entries must not be renumbered and
// numeric values must never be reused.
// LINT.IfChange(FirmwareVmVendor)
enum class FirmwareVmVendor {
  // Firmware strings look like a physical machine.
  kNone = 0,
  // Firmware strings indicate a VM of an unrecognized vendor.
  kUnknown = 1,
  // Firmware tables could not be read. "Don't know", not "physical machine".
  kReadFailed = 2,
  kHyperV = 3,
  kVMware = 4,
  kVirtualBox = 5,
  kQemuKvm = 6,
  kXen = 7,
  kParallels = 8,
  kBhyve = 9,
  kGoogleComputeEngine = 10,
  kAmazonEc2 = 11,
  kAlibabaCloud = 12,
  kOpenStack = 13,
  kNutanix = 14,
  kAppleVirtualization = 15,
  kMaxValue = kAppleVirtualization,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:FirmwareVmVendor)

// Which implementations concluded that this is a VM guest.
//
// These values are persisted to logs. Entries must not be renumbered and
// numeric values must never be reused.
// LINT.IfChange(VmDetectionAgreement)
enum class VmDetectionAgreement {
  kNeither = 0,
  kCpuidOnly = 1,
  // Expected for nested virtualization and on ARM64.
  kSmbiosOnly = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:VmDetectionAgreement)

// Returns the hypervisor advertised via CPUID.
HypervisorType GetHypervisorType();

// Returns true if CPUID indicates a VM guest. False for the Hyper-V root
// partition and on ARM64.
bool IsRunningInVirtualMachineCpuid();

// Returns the virtualization vendor advertised by the firmware. Not usable
// from sandboxed processes.
FirmwareVmVendor GetFirmwareVmVendor();

// Returns true if the firmware strings indicate a VM guest. False on a failed
// read.
bool IsRunningInVirtualMachineSmbios();

// The verdicts of both implementations.
struct VmDetectionResult {
  HypervisorType hypervisor_type = HypervisorType::kNone;
  FirmwareVmVendor firmware_vm_vendor = FirmwareVmVendor::kNone;

  // Not derived from the enums above, since `kHyperVRoot`,
  // `kUnsupportedArchitecture` and `kReadFailed` all map to false.
  bool cpuid_says_vm = false;
  bool smbios_says_vm = false;
};

// Runs both implementations. Must not be called from a sandboxed process.
VmDetectionResult DetectVirtualMachine();

// Maps the two verdicts onto an agreement bucket.
VmDetectionAgreement ComputeAgreement(bool cpuid_says_vm, bool smbios_says_vm);

// Records the Windows.VmDetection.* histograms from `result`.
void RecordVirtualMachineHistograms(const VmDetectionResult& result);

// If `kVmDetectionExperiment` is enabled, runs `DetectVirtualMachine()` on a
// `MayBlock()` thread-pool task and records the histograms.
void RecordVirtualMachineHistogramsAsync();

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_H_
