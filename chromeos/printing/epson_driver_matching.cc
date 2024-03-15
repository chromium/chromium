// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/epson_driver_matching.h"

#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chromeos/printing/ppd_provider.h"

namespace chromeos {

bool CanUseEpsonGenericPPD(const PrinterSearchData& sd) {
  // Only matches USB printers.
  if (sd.discovery_type != PrinterSearchData::PrinterDiscoveryType::kUsb) {
    return false;
  }

  // Needed to check if its an Epson printer.
  if (sd.make_and_model.empty()) {
    return false;
  }

  // Fail if this isn't an Epson printer.
  // Note: Assumes make and model strings are already lowercase.
  if (base::ranges::none_of(sd.make_and_model, [](std::string_view emm) {
        return emm.find("epson") != std::string_view::npos;
      })) {
    return false;
  }

  // The command set is retrieved from the 'CMD' field of the printer's IEEE
  // 1284 Device ID.
  for (std::string_view format : sd.printer_id.command_set()) {
    if (base::StartsWith(format, "ESCPR")) {
      return true;
    }
  }

  return false;
}

}  // namespace chromeos
