// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/activity_reporter/virtual_machine_win.h"
#include "components/activity_reporter/virtual_machine_win_internal.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <intrin.h>

#include "base/win/windows_version.h"
#endif

namespace activity_reporter {

namespace {

// CPUID.1:ECX[31], the hypervisor-present bit.
constexpr uint32_t kFeatureInformationLeaf = 0x1;
constexpr uint32_t kHypervisorPresentBit = 1u << 31;

// Hypervisor leaf blocks start at 0x4000'0000, every 0x100. Hypervisors that
// emulate Hyper-V (VirtualBox, QEMU/KVM, Xen with viridian) put "Microsoft Hv"
// in the first block and their own signature in a later one, in practice
// 0x4000'0100. Each CPUID traps to the hypervisor, so the scan is kept short.
constexpr uint32_t kFirstHypervisorLeaf = 0x4000'0000;
constexpr uint32_t kLastScannedHypervisorLeaf = 0x4000'0400;
constexpr uint32_t kHypervisorLeafStep = 0x100;

// Hyper-V interface leaves in the first block. EAX of the interface leaf holds
// `internal::kHyperVInterfaceSignature`.
constexpr uint32_t kHyperVInterfaceLeaf = kFirstHypervisorLeaf + 1;
constexpr uint32_t kHyperVPrivilegeMaskLeaf = kFirstHypervisorLeaf + 3;

// Privileges only granted to the root partition, in EBX of leaf 0x4000'0003,
// which holds the high half of HV_PARTITION_PRIVILEGE_MASK (Hyper-V TLFS,
// "Partition Privilege Flags"): CreatePartitions (bit 0) and CpuManagement
// (bit 12). The Linux kernel keys `hv_root_partition` off the latter
// (arch/x86/kernel/cpu/mshyperv.c); either one suffices here.
constexpr uint32_t kHyperVCreatePartitionsPrivilege = 1u << 0;
constexpr uint32_t kHyperVCpuManagementPrivilege = 1u << 12;
constexpr uint32_t kHyperVRootPartitionPrivileges =
    kHyperVCreatePartitionsPrivilege | kHyperVCpuManagementPrivilege;

using internal::CpuidFunction;
using internal::CpuidRegisters;

// Returns the 12-byte vendor signature packed into EBX:ECX:EDX, with trailing
// padding (NULs and spaces) removed. E.g. "KVMKVMKVM\0\0\0" -> "KVMKVMKVM".
std::string VendorSignature(const CpuidRegisters& registers) {
  // The length is explicit because the literal starts with a NUL.
  static constexpr std::string_view kPadding("\0 ", 2);
  const std::array<uint32_t, 3> words = {registers.ebx, registers.ecx,
                                         registers.edx};
  return std::string(
      base::TrimString(base::as_string_view(base::as_chars(base::span(words))),
                       kPadding, base::TRIM_TRAILING));
}

// Returns the vendor of the block at `leaf`, or std::nullopt if unrecognized
// or if the block is not implemented. A block that is implemented reports its
// highest leaf, within the block, in EAX. An unimplemented leaf may instead
// return stale data, e.g. KVM answers with the highest basic leaf.
std::optional<HypervisorType> VendorAtLeaf(CpuidFunction cpuid, uint32_t leaf) {
  const CpuidRegisters registers = cpuid(leaf);
  if (registers.eax < leaf || registers.eax >= leaf + kHypervisorLeafStep) {
    return std::nullopt;
  }
  return internal::HypervisorTypeFromVendorSignature(
      VendorSignature(registers));
}

// Returns the first recognized vendor found past the first block. "Microsoft
// Hv" is skipped: it only identifies Hyper-V when it is in the first block, and
// hypervisors emulating Hyper-V put it there, never in a later one.
std::optional<HypervisorType> VendorInLaterBlocks(CpuidFunction cpuid) {
  for (uint32_t leaf = kFirstHypervisorLeaf + kHypervisorLeafStep;
       leaf <= kLastScannedHypervisorLeaf; leaf += kHypervisorLeafStep) {
    const std::optional<HypervisorType> type = VendorAtLeaf(cpuid, leaf);
    if (type.has_value() && *type != HypervisorType::kHyperVGuest) {
      return type;
    }
  }
  return std::nullopt;
}

// Returns true if the Hyper-V interface in the first block, whose highest leaf
// is `max_leaf`, grants a root-only privilege, i.e. this is the root partition.
bool IsHyperVRootPartition(CpuidFunction cpuid, uint32_t max_leaf) {
  if (max_leaf < kHyperVPrivilegeMaskLeaf) {
    return false;
  }
  if (cpuid(kHyperVInterfaceLeaf).eax != internal::kHyperVInterfaceSignature) {
    return false;
  }
  return (cpuid(kHyperVPrivilegeMaskLeaf).ebx &
          kHyperVRootPartitionPrivileges) != 0;
}

#if defined(ARCH_CPU_X86_FAMILY)
CpuidRegisters Cpuid(uint32_t leaf) {
  std::array<int, 4> registers = {};
  __cpuidex(registers.data(), static_cast<int>(leaf), /*subleaf=*/0);
  return CpuidRegisters{
      static_cast<uint32_t>(registers[0]), static_cast<uint32_t>(registers[1]),
      static_cast<uint32_t>(registers[2]), static_cast<uint32_t>(registers[3])};
}
#endif  // defined(ARCH_CPU_X86_FAMILY)

HypervisorType ComputeHypervisorType() {
#if defined(ARCH_CPU_X86_FAMILY)
  // Under x86/x64 emulation on ARM64, CPUID values come from the emulator.
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64()) {
    return HypervisorType::kUnsupportedArchitecture;
  }
  return internal::HypervisorTypeFromCpuid(&Cpuid);
#else
  // No CPUID on ARM64: "don't know", not "physical machine".
  return HypervisorType::kUnsupportedArchitecture;
#endif  // defined(ARCH_CPU_X86_FAMILY)
}

}  // namespace

namespace internal {

std::optional<HypervisorType> HypervisorTypeFromVendorSignature(
    std::string_view signature) {
  // Sources: Hyper-V TLFS, Linux arch/x86/kernel/cpu/hypervisor.c, QEMU
  // hyperv.c (KVM's default Hyper-V vendor ID is "Linux KVM Hv") and systemd
  // detect-virt.c.
  static constexpr auto kSignatures =
      base::MakeFixedFlatMap<std::string_view, HypervisorType>({
          {"Microsoft Hv", HypervisorType::kHyperVGuest},
          {"VMwareVMware", HypervisorType::kVMware},
          {"VBoxVBoxVBox", HypervisorType::kVirtualBox},
          {"KVMKVMKVM", HypervisorType::kKvm},
          {"Linux KVM Hv", HypervisorType::kKvm},
          {"TCGTCGTCGTCG", HypervisorType::kQemuTcg},
          {"XenVMMXenVMM", HypervisorType::kXen},
          // "prl hyperv  ", trailing spaces trimmed.
          {"prl hyperv", HypervisorType::kParallels},
          // "prl hyperv  " with each 32-bit word byte-swapped. Inner spaces
          // are significant.
          {" lrpepyh  vr", HypervisorType::kParallels},
          {"bhyve bhyve", HypervisorType::kBhyve},
          {"ACRNACRNACRN", HypervisorType::kAcrn},
      });
  return base::OptionalFromPtr(base::FindOrNull(kSignatures, signature));
}

HypervisorType HypervisorTypeFromCpuid(CpuidFunction cpuid) {
  // Also set on the Hyper-V root partition, which is handled below.
  if ((cpuid(kFeatureInformationLeaf).ecx & kHypervisorPresentBit) == 0) {
    return HypervisorType::kNone;
  }

  const CpuidRegisters first_block = cpuid(kFirstHypervisorLeaf);
  const std::optional<HypervisorType> type =
      HypervisorTypeFromVendorSignature(VendorSignature(first_block));

  if (!type.has_value()) {
    // Unrecognized first block: try the later ones.
    return VendorInLaterBlocks(cpuid).value_or(HypervisorType::kUnknown);
  }

  // A non-Hyper-V vendor in the first block is final.
  if (*type != HypervisorType::kHyperVGuest) {
    return *type;
  }

  // "Microsoft Hv" is Hyper-V or another hypervisor emulating it. Either way,
  // check for the root partition first, then look for the real vendor.
  if (IsHyperVRootPartition(cpuid, first_block.eax)) {
    return HypervisorType::kHyperVRoot;
  }
  return VendorInLaterBlocks(cpuid).value_or(HypervisorType::kHyperVGuest);
}

bool HypervisorTypeIndicatesVm(HypervisorType type) {
  switch (type) {
    case HypervisorType::kNone:
    case HypervisorType::kUnsupportedArchitecture:
    case HypervisorType::kHyperVRoot:
    // May be a physical host under a security hypervisor: "don't know".
    case HypervisorType::kUnknown:
      return false;
    case HypervisorType::kHyperVGuest:
    case HypervisorType::kVMware:
    case HypervisorType::kVirtualBox:
    case HypervisorType::kKvm:
    case HypervisorType::kQemuTcg:
    case HypervisorType::kXen:
    case HypervisorType::kParallels:
    case HypervisorType::kBhyve:
    case HypervisorType::kAcrn:
      return true;
  }
  NOTREACHED();
}

}  // namespace internal

HypervisorType GetHypervisorType() {
  // The answer cannot change over the lifetime of the process.
  static const HypervisorType type = ComputeHypervisorType();
  return type;
}

bool IsRunningInVirtualMachineCpuid() {
  return internal::HypervisorTypeIndicatesVm(GetHypervisorType());
}

}  // namespace activity_reporter
