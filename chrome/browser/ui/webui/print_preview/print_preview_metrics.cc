// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

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

void ReportPrintDocumentTypeAndSizeHistograms(PrintDocumentTypeBuckets doctype,
                                              size_t average_page_size_in_kb) {
  base::UmaHistogramEnumeration("PrintPreview.PrintDocumentType", doctype);
  switch (doctype) {
    case PrintDocumentTypeBuckets::kHtmlDocument:
      base::UmaHistogramMemoryKB("PrintPreview.PrintDocumentSize.HTML",
                                 average_page_size_in_kb);
      return;
    case PrintDocumentTypeBuckets::kPdfDocument:
      base::UmaHistogramMemoryKB("PrintPreview.PrintDocumentSize.PDF",
                                 average_page_size_in_kb);
      return;
  }
}

void ReportPrintSettingsStats(const base::Value& print_settings,
                              const base::Value& preview_settings,
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
  const base::Value* page_range_array =
      preview_settings.FindKey(kSettingPageRange);
  if (page_range_array && page_range_array->is_list() &&
      !page_range_array->GetList().empty()) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPageRange);
  }

  const base::Value* media_size_value =
      preview_settings.FindKey(kSettingMediaSize);
  if (media_size_value && media_size_value->is_dict() &&
      !media_size_value->DictEmpty()) {
    if (media_size_value->FindBoolKey(kSettingMediaSizeIsDefault)
            .value_or(false)) {
      ReportPrintSettingHistogram(PrintSettingsBuckets::kDefaultMedia);
    } else {
      ReportPrintSettingHistogram(PrintSettingsBuckets::kNonDefaultMedia);
    }
  }

  base::Optional<bool> landscape_opt =
      preview_settings.FindBoolKey(kSettingLandscape);
  if (landscape_opt)
    ReportPrintSettingHistogram(landscape_opt.value()
                                    ? PrintSettingsBuckets::kLandscape
                                    : PrintSettingsBuckets::kPortrait);

  if (print_settings.FindIntKey(kSettingCopies).value_or(1) > 1)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCopies);

  if (preview_settings.FindIntKey(kSettingPagesPerSheet).value_or(1) != 1)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPagesPerSheet);

  if (print_settings.FindBoolKey(kSettingCollate).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCollate);

  base::Optional<int> duplex_mode_opt =
      print_settings.FindIntKey(kSettingDuplexMode);
  if (duplex_mode_opt)
    ReportPrintSettingHistogram(duplex_mode_opt.value()
                                    ? PrintSettingsBuckets::kDuplex
                                    : PrintSettingsBuckets::kSimplex);

  base::Optional<int> color_mode_opt = print_settings.FindIntKey(kSettingColor);
  if (color_mode_opt.has_value()) {
    mojom::ColorModel color_model =
        ColorModeToColorModel(color_mode_opt.value());
    bool unknown_color_model =
        color_model == mojom::ColorModel::kUnknownColorModel;
    if (!unknown_color_model) {
      base::Optional<bool> is_color = IsColorModelSelected(color_model);
      ReportPrintSettingHistogram(is_color.value()
                                      ? PrintSettingsBuckets::kColor
                                      : PrintSettingsBuckets::kBlackAndWhite);
    }

    // Record whether the printing backend does not understand the printer's
    // color capabilities. Do this only once per device.
    static base::NoDestructor<base::flat_set<std::string>> seen_devices;
    auto result =
        seen_devices->insert(*print_settings.FindStringKey(kSettingDeviceName));
    bool is_new_device = result.second;
    if (is_new_device) {
      base::UmaHistogramBoolean("Printing.CUPS.UnknownPpdColorModel",
                                unknown_color_model);
    }
  }

  if (preview_settings.FindIntKey(kSettingMarginsType).value_or(0) != 0)
    ReportPrintSettingHistogram(PrintSettingsBuckets::kNonDefaultMargins);

  if (preview_settings.FindBoolKey(kSettingHeaderFooterEnabled).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kHeadersAndFooters);

  if (preview_settings.FindBoolKey(kSettingShouldPrintBackgrounds)
          .value_or(false)) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kCssBackground);
  }

  if (preview_settings.FindBoolKey(kSettingShouldPrintSelectionOnly)
          .value_or(false)) {
    ReportPrintSettingHistogram(PrintSettingsBuckets::kSelectionOnly);
  }

  if (preview_settings.FindBoolKey(kSettingRasterizePdf).value_or(false))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPrintAsImage);

  ScalingType scaling_type =
      static_cast<ScalingType>(preview_settings.FindIntKey(kSettingScalingType)
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

  if (print_settings.FindIntKey(kSettingDpiHorizontal).value_or(0) > 0 &&
      print_settings.FindIntKey(kSettingDpiVertical).value_or(0) > 0) {
    base::Optional<bool> is_default_opt =
        print_settings.FindBoolKey(kSettingDpiDefault);
    if (is_default_opt) {
      ReportPrintSettingHistogram(is_default_opt.value()
                                      ? PrintSettingsBuckets::kDefaultDpi
                                      : PrintSettingsBuckets::kNonDefaultDpi);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (print_settings.FindStringKey(kSettingPinValue))
    ReportPrintSettingHistogram(PrintSettingsBuckets::kPin);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ReportRegeneratePreviewRequestCountBeforeCancel(size_t count) {
  base::UmaHistogramCounts1M(
      "PrintPreview.RegeneratePreviewRequest.BeforeCancel", count);
}

void ReportRegeneratePreviewRequestCountBeforePrint(size_t count) {
  base::UmaHistogramCounts1M(
      "PrintPreview.RegeneratePreviewRequest.BeforePrint", count);
}

void ReportUserActionHistogram(UserActionBuckets event) {
  // Use macro because this histogram is called multiple times in succession.
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event);
}

}  // namespace printing
