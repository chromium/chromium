// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/virtual_machine_win.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/activity_reporter/virtual_machine_win_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#include "base/win/windows_version.h"
#endif

namespace activity_reporter {

namespace {

constexpr uint32_t kHypervisorBase = 0x4000'0000;

// A CPUID implementation backed by a table of leaves. Unlisted leaves return
// all-zero registers.
class FakeCpuid {
 public:
  FakeCpuid& Set(uint32_t leaf, const internal::CpuidRegisters& registers) {
    leaves_[leaf] = registers;
    return *this;
  }

  // Sets CPUID.1:ECX[31].
  FakeCpuid& SetHypervisorPresent() {
    leaves_[1].ecx |= 1u << 31;
    return *this;
  }

  // Packs `signature` (at most 12 bytes, NUL-padded) into EBX:ECX:EDX of
  // `leaf`, and sets EAX to `max_leaf`, which defaults to `leaf` itself.
  FakeCpuid& SetVendor(uint32_t leaf,
                       std::string_view signature,
                       std::optional<uint32_t> max_leaf = std::nullopt) {
    std::array<uint32_t, 3> words = {};
    base::as_writable_chars(base::span(words))
        .copy_prefix_from(base::span(signature));
    internal::CpuidRegisters& registers = leaves_[leaf];
    registers.eax = max_leaf.value_or(leaf);
    registers.ebx = words[0];
    registers.ecx = words[1];
    registers.edx = words[2];
    return *this;
  }

  // Publishes the "Hv#1" interface at `leaf + 1` and, if `root`, the
  // CreatePartitions privilege at `leaf + 3`.
  FakeCpuid& SetHyperVInterface(uint32_t leaf, bool root) {
    leaves_[leaf + 1].eax = internal::kHyperVInterfaceSignature;
    leaves_[leaf + 3].ebx = root ? 0x1 : 0x0;
    return *this;
  }

  internal::CpuidRegisters operator()(uint32_t leaf) const {
    max_queried_leaf_ = std::max(max_queried_leaf_, leaf);
    const internal::CpuidRegisters* registers = base::FindOrNull(leaves_, leaf);
    return registers ? *registers : internal::CpuidRegisters();
  }

  HypervisorType Run() const {
    return internal::HypervisorTypeFromCpuid(*this);
  }

  uint32_t max_queried_leaf() const { return max_queried_leaf_; }

 private:
  base::flat_map<uint32_t, internal::CpuidRegisters> leaves_;
  mutable uint32_t max_queried_leaf_ = 0;
};

// Builds one SMBIOS structure: a 4-byte header, `formatted_area`, then the
// string set.
std::vector<uint8_t> MakeStructure(
    uint8_t type,
    const std::vector<uint8_t>& formatted_area,
    const std::vector<std::string_view>& strings) {
  std::vector<uint8_t> bytes = {
      type,
      base::checked_cast<uint8_t>(4u + formatted_area.size()),
      0,  // Handle, low byte.
      0,  // Handle, high byte.
  };
  base::Extend(bytes, formatted_area);
  // NUL-separated strings followed by a double NUL; just the double NUL if
  // there are no strings.
  const std::string string_set =
      base::StrCat({base::JoinString(strings, std::string_view("\0", 1)),
                    std::string_view("\0\0", 2)});
  base::Extend(bytes, base::as_byte_span(string_set));
  return bytes;
}

// Wraps `structures` in a RawSMBIOSData header, as GetSystemFirmwareTable()
// returns it.
std::vector<uint8_t> MakeRawSmbiosData(
    const std::vector<std::vector<uint8_t>>& structures) {
  std::vector<uint8_t> table;
  for (const std::vector<uint8_t>& structure : structures) {
    base::Extend(table, structure);
  }

  std::vector<uint8_t> raw_data = {
      0,  // Used20CallingMethod.
      3,  // SMBIOSMajorVersion.
      0,  // SMBIOSMinorVersion.
      0,  // DmiRevision.
  };
  base::Extend(raw_data,
               base::U32ToLittleEndian(static_cast<uint32_t>(table.size())));
  base::Extend(raw_data, std::move(table));
  return raw_data;
}

// A BIOS Information (type 0) structure whose Vendor is `vendor`.
std::vector<uint8_t> MakeBiosStructure(std::string_view vendor) {
  // Formatted area: Vendor (string 1), BIOS Version (string 2), then padding
  // for the fields this code does not read.
  return MakeStructure(/*type=*/0, /*formatted_area=*/{1, 2, 0, 0, 0, 0},
                       {vendor, "1.0"});
}

// A System Information (type 1) structure.
std::vector<uint8_t> MakeSystemStructure(std::string_view manufacturer,
                                         std::string_view product) {
  // Formatted area: Manufacturer (string 1), Product Name (string 2), Version
  // (unset), Serial Number (unset).
  return MakeStructure(/*type=*/1, /*formatted_area=*/{1, 2, 0, 0},
                       {manufacturer, product});
}

FirmwareVmVendor ParseFirmware(std::string_view bios_vendor,
                               std::string_view manufacturer,
                               std::string_view product) {
  const std::vector<uint8_t> raw_data = MakeRawSmbiosData(
      {MakeBiosStructure(bios_vendor),
       MakeSystemStructure(manufacturer, product),
       MakeStructure(/*type=*/127, /*formatted_area=*/{}, /*strings=*/{})});
  const internal::FirmwareStrings strings =
      internal::FirmwareStringsFromRawSmbiosData(raw_data);
  return internal::FirmwareVmVendorFromStrings(
      strings.bios_vendor, strings.system_manufacturer, strings.system_product);
}

}  // namespace

namespace internal {

void PrintTo(const FirmwareStrings& strings, std::ostream* os) {
  *os << "{bios_vendor: \"" << strings.bios_vendor
      << "\", system_manufacturer: \"" << strings.system_manufacturer
      << "\", system_product: \"" << strings.system_product << "\"}";
}

}  // namespace internal

TEST(VirtualMachineCpuidTest, KnownVendorSignatures) {
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("Microsoft Hv"),
            HypervisorType::kHyperVGuest);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("VMwareVMware"),
            HypervisorType::kVMware);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("VBoxVBoxVBox"),
            HypervisorType::kVirtualBox);
  // "KVMKVMKVM\0\0\0" with the trailing padding trimmed.
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("KVMKVMKVM"),
            HypervisorType::kKvm);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("Linux KVM Hv"),
            HypervisorType::kKvm);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("TCGTCGTCGTCG"),
            HypervisorType::kQemuTcg);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("XenVMMXenVMM"),
            HypervisorType::kXen);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("prl hyperv"),
            HypervisorType::kParallels);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature(" lrpepyh  vr"),
            HypervisorType::kParallels);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("bhyve bhyve"),
            HypervisorType::kBhyve);
  EXPECT_EQ(internal::HypervisorTypeFromVendorSignature("ACRNACRNACRN"),
            HypervisorType::kAcrn);
}

TEST(VirtualMachineCpuidTest, UnknownVendorSignatures) {
  EXPECT_FALSE(
      internal::HypervisorTypeFromVendorSignature("GenuineIntel").has_value());
  EXPECT_FALSE(internal::HypervisorTypeFromVendorSignature("").has_value());
  EXPECT_FALSE(
      internal::HypervisorTypeFromVendorSignature("Microsoft").has_value());
  // The inner padding of the byte-swapped Parallels signature is significant.
  EXPECT_FALSE(
      internal::HypervisorTypeFromVendorSignature(" lrpepyh vr").has_value());
}

TEST(VirtualMachineCpuidTest, LiteralRegisterValues) {
  // The well-known register values, written out so that the byte order does
  // not depend on `FakeCpuid::SetVendor()`, which packs the same way as the
  // code under test.
  EXPECT_EQ(internal::kHyperVInterfaceSignature, 0x31237648u);  // "Hv#1".

  // "KVMKVMKVM\0\0\0".
  FakeCpuid kvm;
  kvm.SetHypervisorPresent().Set(kHypervisorBase, {.eax = kHypervisorBase + 1,
                                                   .ebx = 0x4b4d564b,
                                                   .ecx = 0x564b4d56,
                                                   .edx = 0x0000004d});
  EXPECT_EQ(kvm.Run(), HypervisorType::kKvm);

  // "Microsoft Hv" and "Hv#1" with no privileges: a Hyper-V guest.
  FakeCpuid hyperv;
  hyperv.SetHypervisorPresent()
      .Set(kHypervisorBase, {.eax = kHypervisorBase + 6,
                             .ebx = 0x7263694d,
                             .ecx = 0x666f736f,
                             .edx = 0x76482074})
      .Set(kHypervisorBase + 1, {.eax = 0x31237648});
  EXPECT_EQ(hyperv.Run(), HypervisorType::kHyperVGuest);

  // Plus CreatePartitions: the root partition.
  hyperv.Set(kHypervisorBase + 3, {.ebx = 0x1});
  EXPECT_EQ(hyperv.Run(), HypervisorType::kHyperVRoot);
}

TEST(VirtualMachineCpuidTest, NoHypervisorBit) {
  FakeCpuid cpuid;
  // Hypervisor leaves are ignored unless CPUID.1:ECX[31] is set.
  cpuid.SetVendor(kHypervisorBase, "KVMKVMKVM");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kNone);
}

TEST(VirtualMachineCpuidTest, VendorInFirstBlock) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent().SetVendor(kHypervisorBase, "VMwareVMware");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kVMware);
  // Later blocks are not queried.
  EXPECT_EQ(cpuid.max_queried_leaf(), kHypervisorBase);
}

TEST(VirtualMachineCpuidTest, TrailingPaddingIsTrimmed) {
  FakeCpuid kvm;
  // Padded with NULs by SetVendor().
  kvm.SetHypervisorPresent().SetVendor(kHypervisorBase, "KVMKVMKVM");
  EXPECT_EQ(kvm.Run(), HypervisorType::kKvm);

  FakeCpuid parallels;
  parallels.SetHypervisorPresent().SetVendor(kHypervisorBase, "prl hyperv  ");
  EXPECT_EQ(parallels.Run(), HypervisorType::kParallels);

  FakeCpuid parallels_swapped;
  parallels_swapped.SetHypervisorPresent().SetVendor(kHypervisorBase,
                                                     " lrpepyh  vr");
  EXPECT_EQ(parallels_swapped.Run(), HypervisorType::kParallels);
}

TEST(VirtualMachineCpuidTest, HyperVGuest) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false);
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest);
}

TEST(VirtualMachineCpuidTest, HyperVRoot) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/true);
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVRoot);
}

TEST(VirtualMachineCpuidTest, RootRequiresHyperVInterfaceSignature) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/true);
  cpuid.Set(kHypervisorBase + 1, {});  // No "Hv#1".
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest);
}

TEST(VirtualMachineCpuidTest, RootDetectedByEitherRootOnlyPrivilege) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false);

  // CpuManagement (bit 12) alone, as the Linux kernel checks.
  cpuid.Set(kHypervisorBase + 3, {.ebx = 1u << 12});
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVRoot);

  // Privileges that child partitions also get, e.g. AccessPartitionId (bit 1)
  // and AccessMemoryPool (bit 2), do not make a root.
  cpuid.Set(kHypervisorBase + 3, {.ebx = (1u << 1) | (1u << 2)});
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest);
}

TEST(VirtualMachineCpuidTest, RootRequiresPrivilegeLeafInRange) {
  FakeCpuid cpuid;
  // The first block claims to end before the privilege mask leaf.
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 2)
      .SetHyperVInterface(kHypervisorBase, /*root=*/true);
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest);
}

TEST(VirtualMachineCpuidTest, VendorBehindHyperVFacade) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false)
      .SetVendor(kHypervisorBase + 0x100, "VBoxVBoxVBox");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kVirtualBox);
}

TEST(VirtualMachineCpuidTest, RootPartitionIsNotRefinedByLaterBlocks) {
  // A Hyper-V root partition nested in another hypervisor still reports as
  // the root: CPUID cannot see through the nesting.
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/true)
      .SetVendor(kHypervisorBase + 0x100, "VMwareVMware");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVRoot);
}

TEST(VirtualMachineCpuidTest, UnrecognizedFirstBlockFallsBackToLaterBlock) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "AcmeVisor")
      .SetVendor(kHypervisorBase + 0x100, "XenVMMXenVMM");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kXen);
}

TEST(VirtualMachineCpuidTest, UnrecognizedHypervisor) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent().SetVendor(kHypervisorBase, "AcmeVisor");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kUnknown);
}

TEST(VirtualMachineCpuidTest, ScanIsBounded) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "AcmeVisor")
      .SetVendor(kHypervisorBase + 0x500, "VBoxVBoxVBox");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kUnknown);
  EXPECT_EQ(cpuid.max_queried_leaf(), kHypervisorBase + 0x400);
}

TEST(VirtualMachineCpuidTest, EmptySignatureIsUnknown) {
  // Hypervisor-present bit set, but every hypervisor leaf is all zeros.
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent();
  EXPECT_EQ(cpuid.Run(), HypervisorType::kUnknown);
}

TEST(VirtualMachineCpuidTest, UnrecognizedFirstBlockIgnoresLaterHyperV) {
  // "Microsoft Hv" only counts in the first block.
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "AcmeVisor")
      .SetVendor(kHypervisorBase + 0x100, "Microsoft Hv");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kUnknown);
}

TEST(VirtualMachineCpuidTest, HyperVGuestIgnoresLaterHyperVBlock) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false)
      .SetVendor(kHypervisorBase + 0x100, "Microsoft Hv");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest);
}

TEST(VirtualMachineCpuidTest, VendorBehindHyperVFacadeInLastScannedBlock) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false)
      .SetVendor(kHypervisorBase + 0x400, "KVMKVMKVM");
  EXPECT_EQ(cpuid.Run(), HypervisorType::kKvm);
}

TEST(VirtualMachineCpuidTest, LaterBlockWithMaxLeafOutsideBlockIsIgnored) {
  // E.g. KVM answers an unimplemented leaf with the highest basic leaf, whose
  // EAX is small.
  for (uint32_t max_leaf :
       {0x0du, kHypervisorBase + 0xff, kHypervisorBase + 0x200}) {
    FakeCpuid cpuid;
    cpuid.SetHypervisorPresent()
        .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
        .SetHyperVInterface(kHypervisorBase, /*root=*/false)
        .SetVendor(kHypervisorBase + 0x100, "KVMKVMKVM", max_leaf);
    EXPECT_EQ(cpuid.Run(), HypervisorType::kHyperVGuest) << max_leaf;
  }

  FakeCpuid unrecognized;
  unrecognized.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "AcmeVisor")
      .SetVendor(kHypervisorBase + 0x100, "XenVMMXenVMM", /*max_leaf=*/0);
  EXPECT_EQ(unrecognized.Run(), HypervisorType::kUnknown);
}

TEST(VirtualMachineCpuidTest, LaterBlockWithMaxLeafInsideBlockIsAccepted) {
  FakeCpuid cpuid;
  cpuid.SetHypervisorPresent()
      .SetVendor(kHypervisorBase, "Microsoft Hv", kHypervisorBase + 6)
      .SetHyperVInterface(kHypervisorBase, /*root=*/false)
      .SetVendor(kHypervisorBase + 0x100, "KVMKVMKVM", kHypervisorBase + 0x1ff);
  EXPECT_EQ(cpuid.Run(), HypervisorType::kKvm);
}

TEST(VirtualMachineCpuidTest, HypervisorTypeIndicatesVm) {
  // "Don't know" and "physical host" verdicts.
  EXPECT_FALSE(internal::HypervisorTypeIndicatesVm(HypervisorType::kNone));
  EXPECT_FALSE(internal::HypervisorTypeIndicatesVm(HypervisorType::kUnknown));
  EXPECT_FALSE(internal::HypervisorTypeIndicatesVm(
      HypervisorType::kUnsupportedArchitecture));
  EXPECT_FALSE(
      internal::HypervisorTypeIndicatesVm(HypervisorType::kHyperVRoot));

  // Recognized guests.
  for (HypervisorType type :
       {HypervisorType::kHyperVGuest, HypervisorType::kVMware,
        HypervisorType::kVirtualBox, HypervisorType::kKvm,
        HypervisorType::kQemuTcg, HypervisorType::kXen,
        HypervisorType::kParallels, HypervisorType::kBhyve,
        HypervisorType::kAcrn}) {
    EXPECT_TRUE(internal::HypervisorTypeIndicatesVm(type))
        << std::to_underlying(type);
  }
}

#if defined(ARCH_CPU_X86_FAMILY)
TEST(VirtualMachineCpuidTest, ConsistentWithCpuHypervisorBit) {
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64()) {
    // Emulated CPUID is not trusted.
    EXPECT_EQ(GetHypervisorType(), HypervisorType::kUnsupportedArchitecture);
    EXPECT_FALSE(IsRunningInVirtualMachineCpuid());
    return;
  }
  // Anything other than kNone requires the hypervisor-present bit. The
  // converse cannot be asserted: a host running VBS reports kHyperVRoot.
  if (GetHypervisorType() != HypervisorType::kNone) {
    EXPECT_TRUE(base::CPU::GetInstanceNoAllocation().is_running_in_vm());
  } else {
    EXPECT_FALSE(base::CPU::GetInstanceNoAllocation().is_running_in_vm());
  }
  EXPECT_NE(GetHypervisorType(), HypervisorType::kUnsupportedArchitecture);
}
#else
TEST(VirtualMachineCpuidTest, UnsupportedOnArm) {
  EXPECT_EQ(GetHypervisorType(), HypervisorType::kUnsupportedArchitecture);
  EXPECT_FALSE(IsRunningInVirtualMachineCpuid());
}
#endif  // defined(ARCH_CPU_X86_FAMILY)

TEST(VirtualMachineSmbiosTest, RecognizesVirtualMachines) {
  EXPECT_EQ(ParseFirmware("Phoenix Technologies LTD", "VMware, Inc.",
                          "VMware Virtual Platform"),
            FirmwareVmVendor::kVMware);
  EXPECT_EQ(ParseFirmware("innotek GmbH", "innotek GmbH", "VirtualBox"),
            FirmwareVmVendor::kVirtualBox);
  EXPECT_EQ(ParseFirmware("American Megatrends Inc.", "Microsoft Corporation",
                          "Virtual Machine"),
            FirmwareVmVendor::kHyperV);
  EXPECT_EQ(ParseFirmware("SeaBIOS", "QEMU", "Standard PC (Q35 + ICH9, 2009)"),
            FirmwareVmVendor::kQemuKvm);
  // RHEL-derived KVM booted with OVMF: neither "QEMU" nor "SeaBIOS" appears.
  EXPECT_EQ(ParseFirmware("EFI Development Kit II / OVMF", "Red Hat", "KVM"),
            FirmwareVmVendor::kQemuKvm);
  EXPECT_EQ(ParseFirmware("EDK II", "KubeVirt", "None"),
            FirmwareVmVendor::kQemuKvm);
  EXPECT_EQ(ParseFirmware("Xen", "Xen", "HVM domU"), FirmwareVmVendor::kXen);
  EXPECT_EQ(ParseFirmware("Google", "Google", "Google Compute Engine"),
            FirmwareVmVendor::kGoogleComputeEngine);
  EXPECT_EQ(ParseFirmware("Amazon EC2", "Amazon EC2", "t3.medium"),
            FirmwareVmVendor::kAmazonEc2);
  EXPECT_EQ(ParseFirmware("Parallels Software International Inc.",
                          "Parallels International GmbH.", "Parallels ARM VM"),
            FirmwareVmVendor::kParallels);
}

TEST(VirtualMachineSmbiosTest, DoesNotFlagPhysicalMachines) {
  EXPECT_EQ(ParseFirmware("Dell Inc.", "Dell Inc.", "Latitude 7440"),
            FirmwareVmVendor::kNone);
  EXPECT_EQ(ParseFirmware("LENOVO", "LENOVO", "20XW005LUS"),
            FirmwareVmVendor::kNone);
  // A physical Surface also reports "Microsoft Corporation"; only the
  // manufacturer/product pair identifies Hyper-V.
  EXPECT_EQ(ParseFirmware("Microsoft Corporation", "Microsoft Corporation",
                          "Surface Laptop 5"),
            FirmwareVmVendor::kNone);
  EXPECT_EQ(ParseFirmware("Apple Inc.", "Apple Inc.", "Mac14,3"),
            FirmwareVmVendor::kNone);
  // SeaBIOS is also the payload on physical coreboot/Libreboot machines; it is
  // not a VM signal on its own.
  EXPECT_EQ(ParseFirmware("SeaBIOS", "LENOVO", "ThinkPad X230"),
            FirmwareVmVendor::kNone);
}

TEST(VirtualMachineSmbiosTest, UnknownVirtualMachine) {
  EXPECT_EQ(ParseFirmware("Acme BIOS", "Acme Cloud", "Acme Virtual Machine"),
            FirmwareVmVendor::kUnknown);
}

// Documents a known false negative: KVM-based clouds that rebrand every
// firmware string are indistinguishable from an OEM. CPUID still sees KVM, so
// these machines land in the "CPUID only" agreement bucket.
TEST(VirtualMachineSmbiosTest, MissesFullyRebrandedClouds) {
  EXPECT_EQ(ParseFirmware("DigitalOcean", "DigitalOcean", "Droplet"),
            FirmwareVmVendor::kNone);
}

TEST(VirtualMachineSmbiosTest, HandlesStructuresWithoutStrings) {
  // A stringless structure ahead of the System Information structure must not
  // desynchronize the parser.
  const std::vector<uint8_t> raw_data = MakeRawSmbiosData(
      {MakeStructure(/*type=*/42, /*formatted_area=*/{0, 0, 0},
                     /*strings=*/{}),
       MakeSystemStructure("VMware, Inc.", "VMware Virtual Platform"),
       MakeStructure(/*type=*/127, /*formatted_area=*/{}, /*strings=*/{})});
  EXPECT_EQ(
      internal::FirmwareStringsFromRawSmbiosData(raw_data),
      internal::FirmwareStrings({.system_manufacturer = "VMware, Inc.",
                                 .system_product = "VMware Virtual Platform"}));
}

TEST(VirtualMachineSmbiosTest, HandlesMalformedData) {
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData({}),
            internal::FirmwareStrings());

  // Header only.
  const std::vector<uint8_t> header_only = {0, 3, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(header_only),
            internal::FirmwareStrings());

  // Truncated in the middle of a structure's string set. The manufacturer has
  // already been read in full at that point, so it is still recovered, while
  // the partial product string is dropped.
  std::vector<uint8_t> truncated =
      MakeRawSmbiosData({MakeSystemStructure("VMware, Inc.", "VMware")});
  truncated.resize(truncated.size() - 4u);
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(truncated),
            internal::FirmwareStrings({.system_manufacturer = "VMware, Inc."}));

  // A structure that claims a length smaller than its header.
  const std::vector<uint8_t> bad_length = {0, 3, 0, 0, 4, 0, 0, 0, 1, 2, 0, 0};
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(bad_length),
            internal::FirmwareStrings());
}

TEST(VirtualMachineSmbiosTest, StringMatchingIsCaseInsensitive) {
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "VMWARE, INC.", ""),
            FirmwareVmVendor::kVMware);
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "vmware, inc.", ""),
            FirmwareVmVendor::kVMware);
}

TEST(VirtualMachineSmbiosTest, MatchesWholeWordsOnly) {
  // Needles embedded in longer words do not match.
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings(
                "Xenith BIOS", "Maxen Systems", "Workstation"),
            FirmwareVmVendor::kNone);
  EXPECT_EQ(
      internal::FirmwareVmVendorFromStrings("", "Qemurian Ltd.", "Desktop"),
      FirmwareVmVendor::kNone);
  // Punctuation and string boundaries delimit words.
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "Xen", ""),
            FirmwareVmVendor::kXen);
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "VMware, Inc.", ""),
            FirmwareVmVendor::kVMware);
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "", "HVM domU (Xen)"),
            FirmwareVmVendor::kXen);
  // A needle that first occurs inside a word but later as a whole word still
  // matches.
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "Maxen / Xen", ""),
            FirmwareVmVendor::kXen);
}

TEST(VirtualMachineSmbiosTest, MultiWordMatchIgnoresPunctuationAndSpacing) {
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "Microsoft  Corporation",
                                                  "Virtual\tMachine"),
            FirmwareVmVendor::kHyperV);
  EXPECT_EQ(
      internal::FirmwareVmVendorFromStrings("", "", "Google-Compute-Engine"),
      FirmwareVmVendor::kGoogleComputeEngine);
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "", ""),
            FirmwareVmVendor::kNone);
}

TEST(VirtualMachineSmbiosTest, RecognizesMoreVirtualMachines) {
  EXPECT_EQ(ParseFirmware("Apple Inc.", "Apple Inc.",
                          "Apple Virtualization Generic Platform"),
            FirmwareVmVendor::kAppleVirtualization);
  EXPECT_EQ(ParseFirmware("BHYVE", "FreeBSD", "BHYVE"),
            FirmwareVmVendor::kBhyve);
  EXPECT_EQ(ParseFirmware("Bochs", "Bochs", "Bochs"),
            FirmwareVmVendor::kQemuKvm);
  EXPECT_EQ(ParseFirmware("Acme BIOS", "Acme Cloud", "Acme Virtual Platform"),
            FirmwareVmVendor::kUnknown);
}

// Cloud vendors run on KVM/QEMU or Xen and often also report those strings;
// the more specific vendor wins.
TEST(VirtualMachineSmbiosTest, SpecificVendorTakesPrecedence) {
  EXPECT_EQ(ParseFirmware("SeaBIOS", "Alibaba Cloud", "Alibaba Cloud ECS"),
            FirmwareVmVendor::kAlibabaCloud);
  EXPECT_EQ(ParseFirmware("SeaBIOS", "OpenStack Foundation", "OpenStack Nova"),
            FirmwareVmVendor::kOpenStack);
  EXPECT_EQ(ParseFirmware("SeaBIOS", "Nutanix", "AHV"),
            FirmwareVmVendor::kNutanix);
  EXPECT_EQ(ParseFirmware("SeaBIOS", "Google", "Google Compute Engine"),
            FirmwareVmVendor::kGoogleComputeEngine);
  // Xen-based EC2 instances only name Amazon in the System Version string,
  // which is not read, so they classify as Xen. Nitro instances report
  // "Amazon EC2" and are covered by RecognizesVirtualMachines.
  EXPECT_EQ(ParseFirmware("Xen", "Xen", "HVM domU"), FirmwareVmVendor::kXen);
  // Hyper-V is checked before the generic "virtual machine" fallback.
  EXPECT_EQ(ParseFirmware("Hyper-V UEFI Release v4.1", "Microsoft Corporation",
                          "Virtual Machine"),
            FirmwareVmVendor::kHyperV);
}

TEST(VirtualMachineSmbiosTest, HyperVRequiresManufacturerAndProduct) {
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "Microsoft Corporation",
                                                  "Surface Pro 9"),
            FirmwareVmVendor::kNone);
  // A non-Microsoft "Virtual Machine" falls through to the generic match.
  EXPECT_EQ(
      internal::FirmwareVmVendorFromStrings("", "Contoso", "Virtual Machine"),
      FirmwareVmVendor::kUnknown);
}

TEST(VirtualMachineSmbiosTest, IgnoresDataBeyondDeclaredLength) {
  // The header's length covers only the BIOS structure; a System Information
  // structure appended after it must not be parsed.
  std::vector<uint8_t> raw_data =
      MakeRawSmbiosData({MakeBiosStructure("American Megatrends Inc.")});
  base::Extend(raw_data,
               MakeSystemStructure("VMware, Inc.", "VMware Virtual Platform"));
  EXPECT_EQ(
      internal::FirmwareStringsFromRawSmbiosData(raw_data),
      internal::FirmwareStrings({.bios_vendor = "American Megatrends Inc."}));
}

TEST(VirtualMachineSmbiosTest, InvalidDeclaredLengthFallsBackToWholeTable) {
  // A declared length of zero, or one past the end of the buffer, is ignored.
  for (uint32_t declared_length : {0u, 0xffffu}) {
    std::vector<uint8_t> raw_data = MakeRawSmbiosData(
        {MakeSystemStructure("VMware, Inc.", "VMware Virtual Platform")});
    base::span(raw_data).subspan<4u, 4u>().copy_from(
        base::U32ToLittleEndian(declared_length));
    EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(raw_data),
              internal::FirmwareStrings(
                  {.system_manufacturer = "VMware, Inc.",
                   .system_product = "VMware Virtual Platform"}))
        << declared_length;
  }
}

TEST(VirtualMachineSmbiosTest, RecognizesVMwareUefiProductName) {
  // VMware UEFI VMs report the product as e.g. "VMware7,1". "vmware7" is not
  // the whole word "vmware", so detection relies on the manufacturer.
  EXPECT_EQ(ParseFirmware("VMware, Inc.", "VMware, Inc.", "VMware7,1"),
            FirmwareVmVendor::kVMware);
  EXPECT_EQ(internal::FirmwareVmVendorFromStrings("", "", "VMware7,1"),
            FirmwareVmVendor::kNone);
}

TEST(VirtualMachineSmbiosTest, HandlesMissingEndOfTable) {
  const std::vector<uint8_t> raw_data =
      MakeRawSmbiosData({MakeBiosStructure("SeaBIOS"),
                         MakeSystemStructure("QEMU", "Standard PC")});
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(raw_data),
            internal::FirmwareStrings({.bios_vendor = "SeaBIOS",
                                       .system_manufacturer = "QEMU",
                                       .system_product = "Standard PC"}));
}

TEST(VirtualMachineSmbiosTest, IgnoresUnsetAndOutOfRangeStringIndices) {
  // Manufacturer refers to string 5 of 2, product is unset (0).
  const std::vector<uint8_t> raw_data = MakeRawSmbiosData(
      {MakeStructure(/*type=*/1, /*formatted_area=*/{5, 0, 0, 0},
                     {"VMware, Inc.", "VMware Virtual Platform"})});
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(raw_data),
            internal::FirmwareStrings());
}

TEST(VirtualMachineSmbiosTest, UsesFirstStructureOfEachType) {
  const std::vector<uint8_t> raw_data = MakeRawSmbiosData(
      {MakeBiosStructure("Phoenix Technologies LTD"),
       MakeSystemStructure("VMware, Inc.", "VMware Virtual Platform"),
       MakeSystemStructure("Dell Inc.", "Latitude 7440"),
       MakeStructure(/*type=*/127, /*formatted_area=*/{}, /*strings=*/{})});
  EXPECT_EQ(
      internal::FirmwareStringsFromRawSmbiosData(raw_data),
      internal::FirmwareStrings({.bios_vendor = "Phoenix Technologies LTD",
                                 .system_manufacturer = "VMware, Inc.",
                                 .system_product = "VMware Virtual Platform"}));
}

TEST(VirtualMachineSmbiosTest, DropsPartialStringWithoutSeparator) {
  // Truncated inside the first string: no complete string remains, so the
  // partial "VMware" must not be kept.
  std::vector<uint8_t> truncated =
      MakeRawSmbiosData({MakeSystemStructure("VMware, Inc.", "X")});
  // String set is "VMware, Inc.\0X\0\0"; keep only "VMware".
  truncated.resize(truncated.size() - 10u);
  EXPECT_EQ(internal::FirmwareStringsFromRawSmbiosData(truncated),
            internal::FirmwareStrings());
}

TEST(VirtualMachineSmbiosTest, ReadersPreferSmbios) {
  bool registry_read = false;
  EXPECT_EQ(internal::FirmwareVmVendorFromReaders(
                [] {
                  return std::optional<internal::FirmwareStrings>(
                      {.system_manufacturer = "VMware, Inc."});
                },
                [&] {
                  registry_read = true;
                  return std::optional<internal::FirmwareStrings>(
                      {.system_manufacturer = "QEMU"});
                }),
            FirmwareVmVendor::kVMware);
  EXPECT_FALSE(registry_read);
}

TEST(VirtualMachineSmbiosTest, ReadersFallBackToRegistry) {
  const auto registry = [] {
    return std::optional<internal::FirmwareStrings>(
        {.system_manufacturer = "QEMU"});
  };
  // SMBIOS could not be read.
  EXPECT_EQ(
      internal::FirmwareVmVendorFromReaders(
          [] { return std::optional<internal::FirmwareStrings>(); }, registry),
      FirmwareVmVendor::kQemuKvm);
  // SMBIOS was read but held none of the strings.
  EXPECT_EQ(internal::FirmwareVmVendorFromReaders(
                [] {
                  return std::optional<internal::FirmwareStrings>(
                      internal::FirmwareStrings());
                },
                registry),
            FirmwareVmVendor::kQemuKvm);
}

TEST(VirtualMachineSmbiosTest, ReadersReportReadFailed) {
  const auto unreadable = [] {
    return std::optional<internal::FirmwareStrings>();
  };
  const auto empty = [] {
    return std::optional<internal::FirmwareStrings>(
        internal::FirmwareStrings());
  };
  EXPECT_EQ(internal::FirmwareVmVendorFromReaders(unreadable, unreadable),
            FirmwareVmVendor::kReadFailed);
  EXPECT_EQ(internal::FirmwareVmVendorFromReaders(empty, empty),
            FirmwareVmVendor::kReadFailed);
  // Strings that were read but match nothing mean a physical machine, not a
  // failed read.
  EXPECT_EQ(internal::FirmwareVmVendorFromReaders(
                [] {
                  return std::optional<internal::FirmwareStrings>(
                      {.system_manufacturer = "Dell Inc."});
                },
                unreadable),
            FirmwareVmVendor::kNone);
}

TEST(VirtualMachineSmbiosTest, FirmwareVmVendorIndicatesVm) {
  // "Don't know" and "physical machine" verdicts.
  EXPECT_FALSE(internal::FirmwareVmVendorIndicatesVm(FirmwareVmVendor::kNone));
  EXPECT_FALSE(
      internal::FirmwareVmVendorIndicatesVm(FirmwareVmVendor::kReadFailed));

  for (FirmwareVmVendor vendor :
       {FirmwareVmVendor::kUnknown, FirmwareVmVendor::kHyperV,
        FirmwareVmVendor::kVMware, FirmwareVmVendor::kVirtualBox,
        FirmwareVmVendor::kQemuKvm, FirmwareVmVendor::kXen,
        FirmwareVmVendor::kParallels, FirmwareVmVendor::kBhyve,
        FirmwareVmVendor::kGoogleComputeEngine, FirmwareVmVendor::kAmazonEc2,
        FirmwareVmVendor::kAlibabaCloud, FirmwareVmVendor::kOpenStack,
        FirmwareVmVendor::kNutanix, FirmwareVmVendor::kAppleVirtualization}) {
    EXPECT_TRUE(internal::FirmwareVmVendorIndicatesVm(vendor))
        << std::to_underlying(vendor);
  }
}

TEST(VirtualMachineMetricsTest, DetectVirtualMachineMatchesDetectors) {
  const VmDetectionResult result = DetectVirtualMachine();
  EXPECT_EQ(result.hypervisor_type, GetHypervisorType());
  EXPECT_EQ(result.firmware_vm_vendor, GetFirmwareVmVendor());
  EXPECT_EQ(result.cpuid_says_vm, IsRunningInVirtualMachineCpuid());
  EXPECT_EQ(result.smbios_says_vm, IsRunningInVirtualMachineSmbios());
}

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

}  // namespace activity_reporter
