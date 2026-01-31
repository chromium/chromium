// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/usb_printer_id.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_split.h"

namespace chromeos {

// Device ID keys pulled from IEEE Standard 1284.
const char kManufacturer[] = "MANUFACTURER";
const char kManufacturerAbbr[] = "MFG";
const char kModel[] = "MODEL";
const char kModelAbbr[] = "MDL";
const char kCommandSet[] = "COMMAND SET";
const char kCommandSetAbbr[] = "CMD";
const char kChromeOsRawId[] = "CHROMEOS_RAW_ID";

UsbPrinterId::UsbPrinterId(base::span<const uint8_t> device_id_data) {
  // Build mapping.
  id_mappings_ = BuildDeviceIdMapping(device_id_data);

  // Save original ID.
  if (id_mappings_.contains(kChromeOsRawId)) {
    raw_id_ = id_mappings_[kChromeOsRawId].front();
  }

  // Save required mappings.
  // Save make_.
  if (id_mappings_.contains(kManufacturer)) {
    make_ = id_mappings_[kManufacturer].front();
  } else if (id_mappings_.contains(kManufacturerAbbr)) {
    make_ = id_mappings_[kManufacturerAbbr].front();
  }

  // Save model_.
  if (id_mappings_.contains(kModel)) {
    model_ = id_mappings_[kModel].front();
  } else if (id_mappings_.contains(kModelAbbr)) {
    model_ = id_mappings_[kModelAbbr].front();
  }

  // Save command_set_.
  if (id_mappings_.contains(kCommandSet)) {
    command_set_ = id_mappings_[kCommandSet];
  } else if (id_mappings_.contains(kCommandSetAbbr)) {
    command_set_ = id_mappings_[kCommandSetAbbr];
  }
}

UsbPrinterId::UsbPrinterId() = default;
UsbPrinterId::UsbPrinterId(const UsbPrinterId& other) = default;
UsbPrinterId::~UsbPrinterId() = default;

std::map<std::string, std::vector<std::string>> BuildDeviceIdMapping(
    base::span<const uint8_t> data) {
  // Must contain at least the length information.
  if (data.size() < 2) {
    return {};
  }

  std::map<std::string, std::vector<std::string>> ret;

  // Convert to string to work on.
  // Note: First two bytes contain the length, so we skip those.
  std::string printer_id;
  std::copy(data.begin() + 2, data.end(), std::back_inserter(printer_id));

  // We filter out terms with empty keys or values.
  base::StringPairs terms;
  base::SplitStringIntoKeyValuePairs(printer_id, ':', ';', &terms);
  for (const auto& term : terms) {
    if (term.first.empty()) {
      continue;
    }

    std::vector<std::string> values = base::SplitString(
        term.second, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (values.empty()) {
      continue;
    }

    ret[term.first] = values;
  }
  ret[kChromeOsRawId].emplace_back(std::move(printer_id));

  return ret;
}

}  // namespace chromeos
