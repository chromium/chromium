// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_INTERNAL_H_
#define COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_INTERNAL_H_

// Implementation details of virtual_machine_win.h, exposed for testing.

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "components/activity_reporter/virtual_machine_win.h"

namespace activity_reporter::internal {

// "Hv#1", returned in EAX of leaf 0x4000'0001 by the Hyper-V interface.
inline constexpr uint32_t kHyperVInterfaceSignature = 0x31237648;

// The registers returned by the CPUID instruction for one leaf.
struct CpuidRegisters {
  uint32_t eax = 0;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
};

// Executes CPUID for `leaf` (with subleaf 0).
using CpuidFunction = base::FunctionRef<CpuidRegisters(uint32_t leaf)>;

// Maps a CPUID hypervisor vendor signature, trailing NULs and spaces trimmed,
// to a type, or std::nullopt if unrecognized. "Microsoft Hv" always maps to
// `kHyperVGuest`, never `kHyperVRoot`.
std::optional<HypervisorType> HypervisorTypeFromVendorSignature(
    std::string_view signature);

// Identifies the hypervisor using `cpuid`. Never returns
// `kUnsupportedArchitecture`.
HypervisorType HypervisorTypeFromCpuid(CpuidFunction cpuid);

// Returns whether `type` counts as a VM guest.
bool HypervisorTypeIndicatesVm(HypervisorType type);

// The SMBIOS strings used for classification.
struct FirmwareStrings {
  bool empty() const {
    return bios_vendor.empty() && system_manufacturer.empty() &&
           system_product.empty();
  }

  friend bool operator==(const FirmwareStrings&,
                         const FirmwareStrings&) = default;

  std::string bios_vendor;
  std::string system_manufacturer;
  std::string system_product;
};

// Reads the firmware strings from one source, or returns std::nullopt if the
// source could not be read.
using FirmwareStringsReader =
    base::FunctionRef<std::optional<FirmwareStrings>()>;

// Classifies the strings from `read_smbios`, falling back to `read_registry`
// if that yields nothing. Returns `kReadFailed` if neither source yields any
// strings.
FirmwareVmVendor FirmwareVmVendorFromReaders(
    FirmwareStringsReader read_smbios,
    FirmwareStringsReader read_registry);

// Extracts the strings from `raw_smbios_data`, the GetSystemFirmwareTable
// ('RSMB') buffer, header included. Strings that are missing, or that follow a
// malformed or truncated part of the table, are left empty.
FirmwareStrings FirmwareStringsFromRawSmbiosData(
    base::span<const uint8_t> raw_smbios_data);

// Classifies already-extracted firmware strings.
FirmwareVmVendor FirmwareVmVendorFromStrings(
    std::string_view bios_vendor,
    std::string_view system_manufacturer,
    std::string_view system_product);

// Returns whether `vendor` counts as a VM guest.
bool FirmwareVmVendorIndicatesVm(FirmwareVmVendor vendor);

}  // namespace activity_reporter::internal

#endif  // COMPONENTS_ACTIVITY_REPORTER_VIRTUAL_MACHINE_WIN_INTERNAL_H_
