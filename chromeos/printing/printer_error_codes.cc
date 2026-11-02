// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_error_codes.h"

#include "printing/printer_status.h"

namespace chromeos {

using PrinterReason = ::printing::PrinterStatus::PrinterReason;

PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    if (reason.severity != PrinterReason::Severity::kError &&
        reason.severity != PrinterReason::Severity::kWarning) {
      continue;
    }

    switch (reason.reason) {
      case PrinterReason::Reason::kMediaEmpty:
      case PrinterReason::Reason::kMediaNeeded:
        return PrinterErrorCode::OUT_OF_PAPER;
      case PrinterReason::Reason::kMediaJam:
        return PrinterErrorCode::PAPER_JAM;
      case PrinterReason::Reason::kTonerEmpty:
      case PrinterReason::Reason::kDeveloperEmpty:
      case PrinterReason::Reason::kMarkerSupplyEmpty:
      case PrinterReason::Reason::kMarkerWasteFull:
        return PrinterErrorCode::OUT_OF_INK;
      case PrinterReason::Reason::kTimedOut:
      case PrinterReason::Reason::kShutdown:
        return PrinterErrorCode::PRINTER_UNREACHABLE;
      case PrinterReason::Reason::kDoorOpen:
      case PrinterReason::Reason::kCoverOpen:
      case PrinterReason::Reason::kInterlockOpen:
        return PrinterErrorCode::DOOR_OPEN;
      case PrinterReason::Reason::kInputTrayMissing:
      case PrinterReason::Reason::kOutputTrayMissing:
        return PrinterErrorCode::TRAY_MISSING;
      case PrinterReason::Reason::kOutputAreaFull:
      case PrinterReason::Reason::kOutputAreaAlmostFull:
        return PrinterErrorCode::OUTPUT_FULL;
      case PrinterReason::Reason::kStopping:
      case PrinterReason::Reason::kStoppedPartly:
      case PrinterReason::Reason::kPaused:
      case PrinterReason::Reason::kMovingToPaused:
        return PrinterErrorCode::STOPPED;
      case PrinterReason::Reason::kCupsPkiExpired:
        return PrinterErrorCode::EXPIRED_CERTIFICATE;
      case PrinterReason::Reason::kMediaLow:
      case PrinterReason::Reason::kTonerLow:
      case PrinterReason::Reason::kDeveloperLow:
      case PrinterReason::Reason::kMarkerSupplyLow:
      case PrinterReason::Reason::kMarkerWasteAlmostFull:
      default:
        break;
    }
  }
  return PrinterErrorCode::NO_ERROR;
}

}  // namespace chromeos
