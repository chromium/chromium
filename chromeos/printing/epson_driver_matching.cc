// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/epson_driver_matching.h"

#include <algorithm>

#include "chromeos/printing/ppd_provider.h"

namespace chromeos {

bool CanUseEpsonGenericPPD(const PrinterSearchData& sd) {
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

  switch (sd.discovery_type) {
    case PrinterSearchData::PrinterDiscoveryType::kManual:
      // For manually discovered printers, supported_document_formats is
      // retrieved via an ippGetAttributes query to the printer.
      return base::Contains(sd.supported_document_formats,
                            "application/octet-stream");

    case PrinterSearchData::PrinterDiscoveryType::kUsb: {
      // For USB printers, the command set is retrieved from the 'CMD' field of
      // the printer's IEEE 1284 Device ID.
      for (base::StringPiece format : sd.printer_id.command_set()) {
        if (format.starts_with("ESCPR")) {
          return true;
        }
      }
      return false;
    }

    case PrinterSearchData::PrinterDiscoveryType::kZeroconf:
      // For printers found through mDNS/DNS-SD discovery,
      // supported_document_formats is retrieved via the Printer Description TXT
      // Record(from the key 'pdl').
      return base::Contains(sd.supported_document_formats,
                            "application/vnd.epson.escpr");

    default:
      return false;
  }
}

}  // namespace chromeos
