// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/extension_printer.mojom-shared.h"
#endif

namespace printing {

namespace {

void ReportPrintSettingHistogram(PrintSettingsBuckets setting) {
  // Use macro because this histogram is called multiple times in succession.
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintSettings", setting);
}

}  // namespace

void ReportNumberOfPrinters(size_t number) {
  // Use macro because this histogram is called multiple times in succession.
  UMA_HISTOGRAM_COUNTS_1M("PrintPreview.NumberOfPrinters", number);
}

void ReportPrintDocumentTypeHistograms(PrintDocumentTypeBuckets doctype) {
  base::UmaHistogramEnumeration("PrintPreview.PrintDocumentType", doctype);
}

void ReportPrintSettingsStats(const base::Value::Dict& print_settings,
                              const base::Value::Dict& preview_settings,
                              bool is_pdf) {
  ReportPrintSettingHistogram(PrintSettingsBuckets::kTotal);

  // Print settings can be categorized into 2 groups: settings that are applied
  // via preview generation (page range, selection, headers/footers, background
  // graphics, scaling, layout, page size, pages per sheet, fit to page,
  // margins, rasterize), and settings that are applied at the printer (color,
  // duplex, copies, collate, dpi). The former should be captured from the most
  // recent preview request, as some of them are set to dummy values in the
  // print ticket. Similarly, settings applied at the printer should be pulled
  // from the print ticket, as they may have dummy values in the preview
  // request.
  const base::Value::List* page_range_array =
      preview_settings.FindList(kSettingPageRange);
  if (page_range_array && !page_range_array->empty()) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPageRange);
  }

  const base::Value::Dict* media_size_value =
      preview_settings.FindDict(kSettingMediaSize);
  if (media_size_value && !media_size_value->empty()) {
    if (media_size_value->FindBool(kSettingMediaSizeIsDefault)
            .value_or(false)) {
      ReportPrintSettingHistogram(PrintSettingsBuckets::kDefaultMedia);
    } else {
      ReportPrintSettingHistogram(PrintSettingsBuckets::kNonDefaultMedia);
    }
  }

  std::optional<bool> landscape_opt =
      preview_settings.FindBool(kSettingLandscape);
  if (landscape_opt.has_value()) {
    ReportPrintSettingHistogram(landscape_opt.value()
                                    ? PrintSettingsBuckets::kLandscape
                                    : PrintSettingsBuckets::kPortrait);
  }

  if (print_settings.FindInt(kSettingCopies).value_or(1) > 1)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCopies);

  if (preview_settings.FindInt(kSettingPagesPerSheet).value_or(1) != 1)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPagesPerSheet);

  if (print_settings.FindBool(kSettingCollate).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCollate);

  std::optional<int> duplex_mode_opt =
      print_settings.FindInt(kSettingDuplexMode);
  if (duplex_mode_opt.has_value()) {
    ReportPrintSettingHistogram(duplex_mode_opt.value()
                                    ? PrintSettingsBuckets::kDuplex
                                    : PrintSettingsBuckets::kSimplex);
  }

  std::optional<int> color_mode_opt = print_settings.FindInt(kSettingColor);
  if (color_mode_opt.has_value()) {
    mojom::ColorModel color_model =
        ColorModeToColorModel(color_mode_opt.value());
    bool unknown_color_model =
        color_model == mojom::ColorModel::kUnknownColorModel;
    if (!unknown_color_model) {
      std::optional<bool> is_color = IsColorModelSelected(color_model);
      ReportPrintSettingHistogram(is_color.value()
                                      ? PrintSettingsBuckets::kColor
                                      : PrintSettingsBuckets::kBlackAndWhite);
    }

    // Record whether the printing backend does not understand the printer's
    // color capabilities. Do this only once per device.
    static base::NoDestructor<base::flat_set<std::string>> seen_devices;
    auto result =
        seen_devices->insert(*print_settings.FindString(kSettingDeviceName));
    bool is_new_device = result.second;
    if (is_new_device) {
      base::UmaHistogramBoolean("Printing.CUPS.UnknownPpdColorModel",
                                unknown_color_model);
    }
  }

  if (preview_settings.FindInt(kSettingMarginsType).value_or(0) != 0)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kNonDefaultMargins);

  if (preview_settings.FindBool(kSettingHeaderFooterEnabled).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kHeadersAndFooters);

  if (preview_settings.FindBool(kSettingShouldPrintBackgrounds)
          .value_or(false)) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCssBackground);
  }

  if (preview_settings.FindBool(kSettingShouldPrintSelectionOnly)
          .value_or(false)) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kSelectionOnly);
  }

  if (preview_settings.FindBool(kSettingRasterizePdf).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPrintAsImage);

  ScalingType scaling_type =
      static_cast<ScalingType>(preview_settings.FindInt(kSettingScalingType)
                                   .value_or(ScalingType::DEFAULT));
  if (scaling_type == ScalingType::CUSTOM) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kScaling);
  }

  if (is_pdf) {
    if (scaling_type == ScalingType::FIT_TO_PAGE)
      ReportPrintSettingHistogram(PrintSettingsBuckets::kFitToPage);
    else if (scaling_type == ScalingType::FIT_TO_PAPER)
      ReportPrintSettingHistogram(PrintSettingsBuckets::kFitToPaper);
  }

  int dpi_horizontal =
      print_settings.FindInt(kSettingDpiHorizontal).value_or(0);
  int dpi_vertical = print_settings.FindInt(kSettingDpiVertical).value_or(0);
  if (dpi_horizontal > 0 && dpi_vertical > 0) {
    std::optional<bool> is_default_opt =
        print_settings.FindBool(kSettingDpiDefault);
    if (is_default_opt.has_value()) {
      ReportPrintSettingHistogram(is_default_opt.value()
                                      ? PrintSettingsBuckets::kDefaultDpi
                                      : PrintSettingsBuckets::kNonDefaultDpi);
    }
    if (dpi_horizontal != dpi_vertical) {
      ReportPrintSettingHistogram(PrintSettingsBuckets::kNonSquarePixels);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_settings.FindString(kSettingPinValue))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPin);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ReportUserActionHistogram(UserActionBuckets event) {
  // Use macro because this histogram is called multiple times in succession.
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event);
}

void RecordGetPrintersTimeHistogram(mojom::PrinterType printer_type,
                                    const base::TimeTicks& start_time) {
  std::string printer_type_metric;
  switch (printer_type) {
    case mojom::PrinterType::kExtension:
      printer_type_metric = "Extension";
      break;
    case mojom::PrinterType::kPdf:
      printer_type_metric = "PDF";
      break;
    case mojom::PrinterType::kLocal:
      printer_type_metric = "Local";
      break;
    case mojom::PrinterType::kPrivetDeprecated:
    case mojom::PrinterType::kCloudDeprecated:
      NOTREACHED();
  }
  base::UmaHistogramCustomTimes(
      base::StrCat({"PrintPreview.GetPrintersTime.", printer_type_metric}),
      /*sample=*/base::TimeTicks::Now() - start_time,
      /*min=*/base::Milliseconds(1),
      /*max=*/base::Minutes(1), /*buckets=*/50);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ReportLacrosExtensionPrintJobStatusFromAshHistogram(
    crosapi::mojom::StartPrintStatus status) {
  base::UmaHistogramEnumeration("Printing.LacrosExtensions.FromAsh.Job.Result",
                                status);
}
#endif

}  // namespace printing
