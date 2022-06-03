// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/epson_driver_matching.h"

#include <algorithm>

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
  auto it = std::find_if(sd.make_and_model.begin(), sd.make_and_model.end(),
                         [](base::StringPiece emm) {
                           return emm.find("epson") != base::StringPiece::npos;
                         });
  if (it == sd.make_and_model.end()) {
    return false;
  }

  // The command set is retrieved from the 'CMD' field of the printer's IEEE
  // 1284 Device ID.
  for (base::StringPiece format : sd.printer_id.command_set()) {
    if (base::StartsWith(format, "ESCPR")) {
      return true;
    }
  }

  return false;
}

}  // namespace chromeos
