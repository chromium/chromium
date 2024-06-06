// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_

#include <cstddef>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "printing/mojom/print.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/extension_printer.mojom-forward.h"
#endif

namespace base {
class TimeTicks;
}  // namespace base

namespace printing {

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class PrintDocumentTypeBuckets {
  kHtmlDocument = 0,
  kPdfDocument = 1,
  kMaxValue = kPdfDocument
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class PrintSettingsBuckets {
  kLandscape = 0,
  kPortrait = 1,
  kColor = 2,
  kBlackAndWhite = 3,
  kCollate = 4,
  kSimplex = 5,
  kDuplex = 6,
  kTotal = 7,
  kHeadersAndFooters = 8,
  kCssBackground = 9,
  kSelectionOnly = 10,
  // kExternalPdfPreview = 11,  // no longer used
  kPageRange = 12,
  kDefaultMedia = 13,
  kNonDefaultMedia = 14,
  kCopies = 15,
  kNonDefaultMargins = 16,
  // kDistillPage = 17,  // no longer used
  kScaling = 18,
  kPrintAsImage = 19,
  kPagesPerSheet = 20,
  kFitToPage = 21,
  kDefaultDpi = 22,
  kNonDefaultDpi = 23,
  kPin = 24,
  kFitToPaper = 25,
  kNonSquarePixels = 26,
  kMaxValue = kNonSquarePixels
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum class UserActionBuckets {
  kPrintToPrinter = 0,
  kPrintToPdf = 1,
  kCancel = 2,
  kFallbackToAdvancedSettingsDialog = 3,
  kPreviewFailed = 4,
  kPreviewStarted = 5,
  // kInitiatorCrashed = 6,  // no longer used
  kInitiatorClosed = 7,
  kPrintWithCloudPrint = 8,
  // kPrintWithPrivet = 9,  // no longer used
  kPrintWithExtension = 10,
  kOpenInMacPreview = 11,
  kPrintToGoogleDrive = 12,
  kPrintToGoogleDriveCros = 13,
  kMaxValue = kPrintToGoogleDriveCros
};

// Record the number of local printers.
void ReportNumberOfPrinters(size_t number);

void ReportPrintDocumentTypeHistograms(PrintDocumentTypeBuckets doctype);

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const base::Value::Dict& print_settings,
                              const base::Value::Dict& preview_settings,
                              bool is_pdf);

void ReportUserActionHistogram(UserActionBuckets event);

void RecordGetPrintersTimeHistogram(mojom::PrinterType printer_type,
                                    const base::TimeTicks& start_time);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Record the print job status sending to lacros extension printers from ash.
void ReportLacrosExtensionPrintJobStatusFromAshHistogram(
    crosapi::mojom::StartPrintStatus status);
#endif

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_METRICS_H_
