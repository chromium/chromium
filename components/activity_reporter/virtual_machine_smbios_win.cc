// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "components/activity_reporter/virtual_machine_win.h"
#include "components/activity_reporter/virtual_machine_win_internal.h"

namespace activity_reporter {

namespace {

// 'RSMB', the raw SMBIOS firmware table provider, in the big-endian order
// GetSystemFirmwareTable() expects.
constexpr DWORD kRawSmbiosProvider = 0x52534D42;

// Size of the RawSMBIOSData header that precedes the SMBIOS table data:
//   uint8_t Used20CallingMethod, SMBIOSMajorVersion, SMBIOSMinorVersion,
//           DmiRevision;
//   uint32_t Length;
constexpr size_t kRawSmbiosHeaderSize = 8;

// SMBIOS structure types (DMTF DSP0134).
constexpr uint8_t kBiosInformationType = 0;
constexpr uint8_t kSystemInformationType = 1;
constexpr uint8_t kEndOfTableType = 127;

// Size of the type/length/handle header common to every SMBIOS structure.
constexpr size_t kSmbiosStructureHeaderSize = 4;

using internal::FirmwareStrings;

// Consumes the NUL-separated, double-NUL-terminated string set that follows an
// SMBIOS structure's formatted area. The returned views point into `reader`'s
// buffer.
std::vector<std::string_view> ReadStringSet(
    base::SpanReader<const uint8_t>& reader) {
  // The lengths are explicit because the literals consist of NULs.
  static constexpr std::string_view kSeparator("\0", 1);
  static constexpr std::string_view kTerminator("\0\0", 2);

  const std::string_view remaining =
      base::as_string_view(reader.remaining_span());
  std::string_view string_set;
  // A structure with no strings at all is just the terminator, found at 0.
  if (const size_t end = remaining.find(kTerminator);
      end != std::string_view::npos) {
    string_set = remaining.substr(0, end);
    reader.Skip(end + kTerminator.size());
  } else {
    // Truncated table: keep the complete strings, drop a partial last one.
    const size_t last_separator = remaining.rfind(kSeparator);
    string_set = remaining.substr(
        0, last_separator == std::string_view::npos ? 0 : last_separator);
    reader.Skip(remaining.size());
  }
  return base::SplitStringPiece(string_set, kSeparator, base::KEEP_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Returns the string at the 1-based index held in `formatted_area` at
// `offset_in_formatted_area`, or an empty string if unset or out of range.
std::string_view StringAt(base::span<const uint8_t> formatted_area,
                          size_t offset_in_formatted_area,
                          base::span<const std::string_view> strings) {
  if (offset_in_formatted_area >= formatted_area.size()) {
    return {};
  }
  const uint8_t index = formatted_area[offset_in_formatted_area];
  if (index == 0 || index > strings.size()) {
    return {};
  }
  return strings[index - 1u];
}

// Extracts the BIOS vendor (type 0) and system manufacturer/product (type 1)
// from the SMBIOS table, header excluded. Only the first of each type is used.
FirmwareStrings ParseSmbiosTableData(base::span<const uint8_t> table) {
  FirmwareStrings result;
  bool seen_bios_information = false;
  bool seen_system_information = false;
  base::SpanReader<const uint8_t> reader(table);
  while (reader.remaining() >= kSmbiosStructureHeaderSize) {
    uint8_t type = 0;
    uint8_t length = 0;
    if (!reader.ReadU8NativeEndian(type) ||
        !reader.ReadU8NativeEndian(length) ||
        !reader.Skip(sizeof(uint16_t))) {  // Handle.
      break;
    }
    if (length < kSmbiosStructureHeaderSize) {
      // Malformed structure; the rest of the table cannot be trusted.
      break;
    }
    const std::optional<base::span<const uint8_t>> formatted_area =
        reader.Read(length - kSmbiosStructureHeaderSize);
    if (!formatted_area.has_value()) {
      break;
    }
    const std::vector<std::string_view> strings = ReadStringSet(reader);

    // Offsets below are relative to the start of the formatted area, i.e. the
    // spec's offsets minus the 4-byte header.
    switch (type) {
      case kBiosInformationType:
        if (!seen_bios_information) {
          seen_bios_information = true;
          // 0x04: Vendor.
          result.bios_vendor = StringAt(*formatted_area, 0x00, strings);
        }
        break;
      case kSystemInformationType:
        if (!seen_system_information) {
          seen_system_information = true;
          // 0x04: Manufacturer, 0x05: Product Name.
          result.system_manufacturer = StringAt(*formatted_area, 0x00, strings);
          result.system_product = StringAt(*formatted_area, 0x01, strings);
        }
        break;
      case kEndOfTableType:
        return result;
      default:
        break;
    }
    if (seen_bios_information && seen_system_information) {
      return result;
    }
  }
  return result;
}

// Strips the RawSMBIOSData header and trims the rest to the declared length,
// if valid.
base::span<const uint8_t> SmbiosTableData(
    base::span<const uint8_t> raw_smbios_data) {
  if (raw_smbios_data.size() <= kRawSmbiosHeaderSize) {
    return base::span<const uint8_t>();
  }
  // Bytes 4..7 hold the length of the table data that follows the header.
  const uint32_t declared_length =
      base::U32FromLittleEndian(raw_smbios_data.subspan<4u, 4u>());
  const base::span<const uint8_t> table =
      raw_smbios_data.subspan(kRawSmbiosHeaderSize);
  if (declared_length != 0 && declared_length <= table.size()) {
    return table.first(declared_length);
  }
  return table;
}

// Reads the SMBIOS table via GetSystemFirmwareTable(). Returns std::nullopt if
// the table could not be read.
std::optional<FirmwareStrings> ReadFirmwareStringsFromSmbios() {
  const UINT size = ::GetSystemFirmwareTable(kRawSmbiosProvider, 0, nullptr, 0);
  if (size <= kRawSmbiosHeaderSize) {
    return std::nullopt;
  }
  auto buffer = base::HeapArray<uint8_t>::Uninit(size);
  const UINT written =
      ::GetSystemFirmwareTable(kRawSmbiosProvider, 0, buffer.data(), size);
  if (written == 0 || written > size) {
    return std::nullopt;
  }
  return internal::FirmwareStringsFromRawSmbiosData(buffer.first(written));
}

// Fallback if GetSystemFirmwareTable() fails. The kernel copies the same
// SMBIOS values into HKLM\HARDWARE, an in-memory hive. Returns std::nullopt if
// the key cannot be opened.
//
// base::SysInfo reads the same key, but only exposes the product name
// synchronously (HardwareModelName()); the manufacturer is only available via
// the async GetHardwareInfo(), and the BIOS vendor not at all. It also falls
// back to other keys that are not populated from SMBIOS.
std::optional<FirmwareStrings> ReadFirmwareStringsFromRegistry() {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS",
               KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  const auto read = [&key](const wchar_t* name) {
    std::wstring value;
    if (key.ReadValue(name, &value) != ERROR_SUCCESS) {
      return std::string();
    }
    return base::WideToUTF8(value);
  };
  FirmwareStrings strings;
  strings.bios_vendor = read(L"BIOSVendor");
  strings.system_manufacturer = read(L"SystemManufacturer");
  strings.system_product = read(L"SystemProductName");
  return strings;
}

// The OS readers are not unit tested; the fallback, parsing and matching are
// tested via `internal::`.
FirmwareVmVendor ComputeFirmwareVmVendor() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return internal::FirmwareVmVendorFromReaders(
      &ReadFirmwareStringsFromSmbios, &ReadFirmwareStringsFromRegistry);
}

// Lowercases `value` and turns runs of non-alphanumerics into single spaces,
// with a space at each end, so that whole words can be found by substring
// search. E.g. "VMware, Inc." -> " vmware inc ".
std::string NormalizeForWordMatch(std::string_view value) {
  std::string normalized = base::ToLowerASCII(value);
  std::ranges::replace_if(
      normalized, [](char c) { return !base::IsAsciiAlphaNumeric(c); }, ' ');
  return base::StrCat(
      {" ",
       base::CollapseWhitespaceASCII(normalized,
                                     /*trim_sequences_with_line_breaks=*/false),
       " "});
}

// Returns true if `words` (lowercase, single-space separated) occurs as whole
// words in `haystack`, which must come from `NormalizeForWordMatch()`.
bool ContainsWords(std::string_view haystack, std::string_view words) {
  return haystack.contains(base::StrCat({" ", words, " "}));
}

}  // namespace

namespace internal {

FirmwareVmVendor FirmwareVmVendorFromStrings(
    std::string_view bios_vendor,
    std::string_view system_manufacturer,
    std::string_view system_product) {
  const std::string vendor = NormalizeForWordMatch(bios_vendor);
  const std::string manufacturer = NormalizeForWordMatch(system_manufacturer);
  const std::string product = NormalizeForWordMatch(system_product);

  // Hyper-V (also Azure, Windows Sandbox) needs both strings, since
  // Surface devices also report "Microsoft Corporation".
  if (ContainsWords(manufacturer, "microsoft corporation") &&
      ContainsWords(product, "virtual machine")) {
    return FirmwareVmVendor::kHyperV;
  }

  struct FirmwareSignature {
    std::string_view needle;
    FirmwareVmVendor vendor;
  };
  // Most specific first; the first match wins. Needles match whole words in
  // any of the three strings, so "xen" does not match "Maxen".
  static constexpr auto kSignatures = std::to_array<FirmwareSignature>({
      {"google compute engine", FirmwareVmVendor::kGoogleComputeEngine},
      {"amazon ec2", FirmwareVmVendor::kAmazonEc2},
      {"alibaba cloud", FirmwareVmVendor::kAlibabaCloud},
      {"apple virtualization", FirmwareVmVendor::kAppleVirtualization},
      {"openstack", FirmwareVmVendor::kOpenStack},
      {"nutanix", FirmwareVmVendor::kNutanix},
      {"vmware", FirmwareVmVendor::kVMware},
      {"virtualbox", FirmwareVmVendor::kVirtualBox},
      {"innotek gmbh", FirmwareVmVendor::kVirtualBox},
      {"parallels", FirmwareVmVendor::kParallels},
      {"bhyve", FirmwareVmVendor::kBhyve},
      {"kvm", FirmwareVmVendor::kQemuKvm},
      {"kubevirt", FirmwareVmVendor::kQemuKvm},
      {"qemu", FirmwareVmVendor::kQemuKvm},
      {"bochs", FirmwareVmVendor::kQemuKvm},
      // "SeaBIOS" is deliberately absent: it is also the BIOS on physical
      // coreboot/Libreboot machines, and QEMU/KVM guests already identify
      // themselves through the System Information strings.
      {"xen", FirmwareVmVendor::kXen},
      // Generic fallbacks for unrecognized vendors.
      {"virtual machine", FirmwareVmVendor::kUnknown},
      {"virtual platform", FirmwareVmVendor::kUnknown},
  });

  const auto it = std::ranges::find_if(
      kSignatures,
      [&](std::string_view needle) {
        return ContainsWords(manufacturer, needle) ||
               ContainsWords(product, needle) || ContainsWords(vendor, needle);
      },
      &FirmwareSignature::needle);
  return it != kSignatures.end() ? it->vendor : FirmwareVmVendor::kNone;
}

FirmwareVmVendor FirmwareVmVendorFromReaders(
    FirmwareStringsReader read_smbios,
    FirmwareStringsReader read_registry) {
  std::optional<FirmwareStrings> strings = read_smbios();
  if (!strings.has_value() || strings->empty()) {
    strings = read_registry();
  }
  if (!strings.has_value() || strings->empty()) {
    // "Don't know", not "physical machine".
    return FirmwareVmVendor::kReadFailed;
  }
  return FirmwareVmVendorFromStrings(strings->bios_vendor,
                                     strings->system_manufacturer,
                                     strings->system_product);
}

FirmwareStrings FirmwareStringsFromRawSmbiosData(
    base::span<const uint8_t> raw_smbios_data) {
  return ParseSmbiosTableData(SmbiosTableData(raw_smbios_data));
}

bool FirmwareVmVendorIndicatesVm(FirmwareVmVendor vendor) {
  switch (vendor) {
    case FirmwareVmVendor::kNone:
    case FirmwareVmVendor::kReadFailed:
      return false;
    case FirmwareVmVendor::kUnknown:
    case FirmwareVmVendor::kHyperV:
    case FirmwareVmVendor::kVMware:
    case FirmwareVmVendor::kVirtualBox:
    case FirmwareVmVendor::kQemuKvm:
    case FirmwareVmVendor::kXen:
    case FirmwareVmVendor::kParallels:
    case FirmwareVmVendor::kBhyve:
    case FirmwareVmVendor::kGoogleComputeEngine:
    case FirmwareVmVendor::kAmazonEc2:
    case FirmwareVmVendor::kAlibabaCloud:
    case FirmwareVmVendor::kOpenStack:
    case FirmwareVmVendor::kNutanix:
    case FirmwareVmVendor::kAppleVirtualization:
      return true;
  }
  NOTREACHED();
}

}  // namespace internal

FirmwareVmVendor GetFirmwareVmVendor() {
  // The answer cannot change over the lifetime of the process.
  static const FirmwareVmVendor vendor = ComputeFirmwareVmVendor();
  return vendor;
}

bool IsRunningInVirtualMachineSmbios() {
  return internal::FirmwareVmVendorIndicatesVm(GetFirmwareVmVendor());
}

}  // namespace activity_reporter
