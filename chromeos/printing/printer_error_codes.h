// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_
#define CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_

#include "base/component_export.h"

namespace printing {
struct PrinterStatus;
}  // namespace printing

namespace chromeos {

// PrinterErrorCode can be derived either from PrinterStatus or JobStateReason.
enum class PrinterErrorCode {
  NO_ERROR,
  PAPER_JAM,
  OUT_OF_PAPER,
  OUT_OF_INK,
  DOOR_OPEN,
  PRINTER_UNREACHABLE,
  TRAY_MISSING,
  OUTPUT_FULL,
  STOPPED,
  FILTER_FAILED,
  UNKNOWN_ERROR,
  CLIENT_UNAUTHORIZED,
  EXPIRED_CERTIFICATE,
};

// Extracts an PrinterErrorCode from PrinterStatus#reasons. Returns NO_ERROR if
// there are no reasons which indicate an error.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_
