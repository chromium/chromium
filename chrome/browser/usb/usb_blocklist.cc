// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_blocklist.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace {

static base::LazyInstance<UsbBlocklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

constexpr uint16_t kMaxVersion = 0xffff;

// Returns true if the passed string is exactly 4 digits long and only contains
// valid hexadecimal characters (no leading 0x).
bool IsHexComponent(std::string_view string) {
  if (string.length() != 4)
    return false;

  // This is necessary because base::HexStringToUInt allows whitespace and the
  // "0x" prefix in its input.
  for (char c : string) {
    if (c >= '0' && c <= '9')
      continue;
    if (c >= 'a' && c <= 'f')
      continue;
    if (c >= 'A' && c <= 'F')
      continue;
    return false;
  }
  return true;
}

bool CompareEntry(const UsbBlocklist::Entry& a, const UsbBlocklist::Entry& b) {
  return std::tie(a.vendor_id, a.product_id, a.max_version) <
         std::tie(b.vendor_id, b.product_id, b.max_version);
}

// Returns true if an entry in (begin, end] matches the vendor and product IDs
// of |entry| and has a device version greater than or equal to |entry|.
template <class Iterator>
bool EntryMatches(Iterator begin,
                  Iterator end,
                  const UsbBlocklist::Entry& entry) {
  auto it = std::lower_bound(begin, end, entry, CompareEntry);
  return it != end && it->vendor_id == entry.vendor_id &&
         it->product_id == entry.product_id;
}

// This list must be sorted according to CompareEntry.
constexpr UsbBlocklist::Entry kStaticEntries[] = {
    {0x096e, 0x0850, kMaxVersion},  // KEY-ID
    {0x096e, 0x0852, kMaxVersion},  // Feitian
    {0x096e, 0x0853, kMaxVersion},  // Feitian
    {0x096e, 0x0854, kMaxVersion},  // Feitian
    {0x096e, 0x0856, kMaxVersion},  // Feitian
    {0x096e, 0x0858, kMaxVersion},  // Feitian USB+NFC
    {0x096e, 0x085a, kMaxVersion},  // Feitian
    {0x096e, 0x085b, kMaxVersion},  // Feitian
    {0x096e, 0x0880, kMaxVersion},  // HyperFIDO

    {0x09c3, 0x0023, kMaxVersion},  // HID Global BlueTrust Token

    // Yubikey devices. https://crbug.com/818807
    {0x1050, 0x0010, kMaxVersion},
    {0x1050, 0x0018, kMaxVersion},
    {0x1050, 0x0030, kMaxVersion},
    {0x1050, 0x0110, kMaxVersion},
    {0x1050, 0x0111, kMaxVersion},
    {0x1050, 0x0112, kMaxVersion},
    {0x1050, 0x0113, kMaxVersion},
    {0x1050, 0x0114, kMaxVersion},
    {0x1050, 0x0115, kMaxVersion},
    {0x1050, 0x0116, kMaxVersion},
    {0x1050, 0x0120, kMaxVersion},
    {0x1050, 0x0200, kMaxVersion},
    {0x1050, 0x0211, kMaxVersion},
    {0x1050, 0x0401, kMaxVersion},
    {0x1050, 0x0402, kMaxVersion},
    {0x1050, 0x0403, kMaxVersion},
    {0x1050, 0x0404, kMaxVersion},
    {0x1050, 0x0405, kMaxVersion},
    {0x1050, 0x0406, kMaxVersion},
    {0x1050, 0x0407, kMaxVersion},
    {0x1050, 0x0410, kMaxVersion},

    {0x10c4, 0x8acf, kMaxVersion},  // U2F Zero
    {0x18d1, 0x5026, kMaxVersion},  // Titan
    {0x1a44, 0x00bb, kMaxVersion},  // VASCO
    {0x1d50, 0x60fc, kMaxVersion},  // OnlyKey
    {0x1e0d, 0xf1ae, kMaxVersion},  // Keydo AES
    {0x1e0d, 0xf1d0, kMaxVersion},  // Neowave Keydo
    {0x1ea8, 0xf025, kMaxVersion},  // Thetis
    {0x20a0, 0x4287, kMaxVersion},  // Nitrokey
    {0x24dc, 0x0101, kMaxVersion},  // JaCarta
    {0x2581, 0xf1d0, kMaxVersion},  // Happlink
    {0x2abe, 0x1002, kMaxVersion},  // Bluink
    {0x2ccf, 0x0880, kMaxVersion},  // Feitian USB, HyperFIDO
};

}  // namespace

UsbBlocklist::~UsbBlocklist() = default;

// static
UsbBlocklist& UsbBlocklist::Get() {
  return g_singleton.Get();
}

bool UsbBlocklist::IsExcluded(const Entry& entry) const {
  return EntryMatches(std::begin(kStaticEntries), std::end(kStaticEntries),
                      entry) ||
         EntryMatches(dynamic_entries_.begin(), dynamic_entries_.end(), entry);
}

bool UsbBlocklist::IsExcluded(
    const device::mojom::UsbDeviceInfo& device_info) const {
  uint16_t device_version = device_info.device_version_major << 8 |
                            device_info.device_version_minor << 4 |
                            device_info.device_version_subminor;
  return IsExcluded(
      Entry{device_info.vendor_id, device_info.product_id, device_version});
}

void UsbBlocklist::ResetToDefaultValuesForTest() {
  dynamic_entries_.clear();
  PopulateWithServerProvidedValues();
}

UsbBlocklist::UsbBlocklist() {
  DCHECK(std::is_sorted(std::begin(kStaticEntries), std::end(kStaticEntries),
                        CompareEntry));
  PopulateWithServerProvidedValues();
}

void UsbBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string =
      base::GetFieldTrialParamValue("WebUSBBlocklist", "blocklist_additions");

  for (const auto& entry :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> components = base::SplitStringPiece(
        entry, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (components.size() != 3 || !IsHexComponent(components[0]) ||
        !IsHexComponent(components[1]) || !IsHexComponent(components[2])) {
      continue;
    }

    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t max_version;
    if (!base::HexStringToUInt(components[0], &vendor_id) ||
        !base::HexStringToUInt(components[1], &product_id) ||
        !base::HexStringToUInt(components[2], &max_version)) {
      continue;
    }

    dynamic_entries_.emplace_back(vendor_id, product_id, max_version);
  }

  std::sort(dynamic_entries_.begin(), dynamic_entries_.end(), CompareEntry);
}
