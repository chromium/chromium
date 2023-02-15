// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
#define CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_

#include <stddef.h>

namespace chromeos {

// This enum is used in histograms, do not remove/renumber entries. If you're
// adding to this enum, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml.
enum class PrintAttemptOutcome {
  kCancelledPrintButtonDisabled = 0,
  kCancelledNoPrintersAvailable,
  kCancelledOtherPrintersAvailable,
  kCancelledPrinterErrorStatus,
  kCancelledPrinterGoodStatus,
  kCancelledPrinterUnknownStatus,
  kPdfPrintAttempted,
  kPrintJobSuccessInitialPrinter,
  kPrintJobSuccessManuallySelectedPrinter,
  kPrintJobFailInitialPrinter,
  kPrintJobFailManuallySelectedPrinter,
  kMaxValue = kPrintJobFailManuallySelectedPrinter,
};

// Maximum size of a PPD file that we will accept, currently 250k.  This number
// is relatively
// arbitrary, we just don't want to try to handle ridiculously huge files.
constexpr size_t kMaxPpdSizeBytes = 250 * 1024;

// Printing protocol schemes.
inline constexpr char kIppScheme[] = "ipp";
inline constexpr char kIppsScheme[] = "ipps";
inline constexpr char kUsbScheme[] = "usb";
inline constexpr char kHttpScheme[] = "http";
inline constexpr char kHttpsScheme[] = "https";
inline constexpr char kSocketScheme[] = "socket";
inline constexpr char kLpdScheme[] = "lpd";

constexpr int kIppPort = 631;
// IPPS commonly uses the HTTPS port despite the spec saying it should use the
// IPP port.
constexpr int kIppsPort = 443;

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
