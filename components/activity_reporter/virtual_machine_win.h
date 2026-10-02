// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_H_
#define COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_H_

// EXPERIMENTAL. Two implementations of "is this process running in a VM
// guest?" on Windows, compared against each other to measure how often they
// disagree. CPUID is cheap but x86-only and cannot see through a nested Hyper-V
// root partition; SMBIOS works on ARM64 and sees through nesting, but needs a
// syscall and relies on string matching, so it misses KVM-based clouds that
// rebrand every firmware string (e.g. "DigitalOcean" / "Droplet"), which then
// show up as "CPUID only".
//
// TODO(crbug.com/563548288): Delete the losing implementation once the
// experiment concludes.
//
// Neither implementation resists a hypervisor that hides itself, so do not use
// them for security or anti-abuse decisions. Only the closed enums below may be
// recorded; raw firmware strings must never be logged or exfiltrated.

#include "base/feature.h"

namespace activity_reporter {

BASE_DECLARE_FEATURE(kVmDetectionExperiment);

// The hypervisor identified via the CPUID hypervisor leaves.
//
// These values are persisted to logs. Entries must not be renumbered and
// numeric values must never be reused.
// LINT.IfChange(HypervisorType)
enum class HypervisorType {
  // No hypervisor present: a physical machine.
  kNone = 0,
  // Hypervisor-present bit set, vendor not recognized. "Don't know": may be a
  // physical host under a security product's hypervisor. Counts as not a VM,
  // unlike `FirmwareVmVendor::kUnknown`.
  kUnknown = 1,
  // ARM64, native (no CPUID) or x86/x64 emulated (synthesized CPUID). "Don't
  // know", not "physical machine".
  kUnsupportedArchitecture = 2,
  // Hyper-V child partition: Hyper-V/Azure VM or Windows Sandbox.
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

// The virtualization vendor identified from the SMBIOS/firmware strings. The
// cloud vendor values (`kGoogleComputeEngine`, `kAmazonEc2`, `kAlibabaCloud`,
// `kNutanix`) can also come from bare-metal machines, which use the same
// firmware strings as that vendor's VMs.
//
// These values are persisted to logs. Entries must not be renumbered and
// numeric values must never be reused.
// LINT.IfChange(FirmwareVmVendor)
enum class FirmwareVmVendor {
  // Firmware strings look like a physical machine.
  kNone = 0,
  // Generic VM strings (e.g. "Virtual Machine") from an unrecognized vendor.
  // Counts as a VM, unlike `HypervisorType::kUnknown`.
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
  // Nitro instances only. Xen-based instances report as `kXen`.
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
  // Expected for nested virtualization, on ARM64 (native or x86/x64 emulated)
  // and when CPUID reports an unrecognized hypervisor.
  kSmbiosOnly = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:VmDetectionAgreement)

// Returns the hypervisor advertised via CPUID.
HypervisorType GetHypervisorType();

// Returns true if CPUID indicates a VM guest. False for the Hyper-V root
// partition, for an unrecognized hypervisor and on ARM64.
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

  // Not derived from the enums above, since `kHyperVRoot`, `kUnknown`,
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
