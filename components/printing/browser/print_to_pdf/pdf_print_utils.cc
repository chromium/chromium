// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"

#include <string_view>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_params.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/url_canon.h"

namespace print_to_pdf {

namespace {

// The max and min value should match the ones in scaling_settings.html.
// Update both files at the same time.
static constexpr double kScaleMaxVal = 200;
static constexpr double kScaleMinVal = 10;
// Set default margin to 1.0cm = ~2/5 of an inch.
static constexpr double kDefaultMarginInMM = 10.0;
static constexpr double kMMPerInch = printing::kMicronsPerMil;
static constexpr double kDefaultMarginInInches =
    kDefaultMarginInMM / kMMPerInch;

}  // namespace

absl::variant<printing::PageRanges, PdfPrintResult> TextPageRangesToPageRanges(
    std::string_view page_range_text) {
  printing::PageRanges page_ranges;
  for (const auto& range_string :
       base::SplitStringPiece(page_range_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    printing::PageRange range;
    if (range_string.find("-") == std::string_view::npos) {
      if (!base::StringToUint(range_string, &range.from))
        return PdfPrintResult::kPageRangeSyntaxError;
      range.to = range.from;
    } else if (range_string == "-") {
      range.from = 1;
      // Set last page to max value so it gets capped with actual
      // page count once it becomes known in renderer during printing.
      static_assert(printing::PageRange::kMaxPage <
                    std::numeric_limits<uint32_t>::max());
      range.to = printing::PageRange::kMaxPage + 1;
    } else if (base::StartsWith(range_string, "-")) {
      range.from = 1;
      if (!base::StringToUint(range_string.substr(1), &range.to))
        return PdfPrintResult::kPageRangeSyntaxError;
    } else if (base::EndsWith(range_string, "-")) {
      // See comment regarding kMaxPage above.
      range.to = printing::PageRange::kMaxPage + 1;
      if (!base::StringToUint(range_string.substr(0, range_string.length() - 1),
                              &range.from)) {
        return PdfPrintResult::kPageRangeSyntaxError;
      }
    } else {
      auto tokens = base::SplitStringPiece(
          range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() != 2 || !base::StringToUint(tokens[0], &range.from) ||
          !base::StringToUint(tokens[1], &range.to)) {
        return PdfPrintResult::kPageRangeSyntaxError;
      }
    }

    if (range.from < 1 || range.from > range.to)
      return PdfPrintResult::kPageRangeInvalidRange;

    // Page numbers are 1-based in the dictionary.
    // Page numbers are 0-based for the print settings.
    page_ranges.push_back({range.from - 1, range.to - 1});
  }
  return page_ranges;
}

absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
GetPrintPagesParams(const GURL& page_url,
                    std::optional<bool> landscape,
                    std::optional<bool> display_header_footer,
                    std::optional<bool> print_background,
                    std::optional<double> scale,
                    std::optional<double> paper_width,
                    std::optional<double> paper_height,
                    std::optional<double> margin_top,
                    std::optional<double> margin_bottom,
                    std::optional<double> margin_left,
                    std::optional<double> margin_right,
                    std::optional<std::string> header_template,
                    std::optional<std::string> footer_template,
                    std::optional<bool> prefer_css_page_size,
                    std::optional<bool> generate_tagged_pdf,
                    std::optional<bool> generate_document_outline) {
  printing::PrintSettings print_settings;
  print_settings.set_dpi(printing::kPointsPerInch);
  print_settings.SetOrientation(landscape.value_or(false));
  print_settings.set_should_print_backgrounds(print_background.value_or(false));
  print_settings.set_display_header_footer(
      display_header_footer.value_or(false));

  if (print_settings.display_header_footer()) {
    GURL::Replacements url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    std::string url = page_url.ReplaceComponents(url_sanitizer).spec();
    print_settings.set_url(base::UTF8ToUTF16(url));
  }

  print_settings.set_scale_factor(scale.value_or(1.0));
  if (print_settings.scale_factor() > kScaleMaxVal / 100 ||
      print_settings.scale_factor() < kScaleMinVal / 100) {
    return "scale is outside of [0.1 - 2] range";
  }

  double margin_left_in_inches = margin_left.value_or(kDefaultMarginInInches);
  double margin_right_in_inches = margin_right.value_or(kDefaultMarginInInches);
  double margin_top_in_inches = margin_top.value_or(kDefaultMarginInInches);
  double margin_bottom_in_inches =
      margin_bottom.value_or(kDefaultMarginInInches);

  if (margin_left_in_inches < 0)
    return "left margin is negative";
  if (margin_right_in_inches < 0)
    return "right margin is negative";
  if (margin_top_in_inches < 0)
    return "top margin is negative";
  if (margin_bottom_in_inches < 0)
    return "bottom margin is negative";

  printing::PageMargins margins_in_points;
  margins_in_points.left =
      base::ClampFloor(margin_left_in_inches * printing::kPointsPerInch);
  margins_in_points.right =
      base::ClampFloor(margin_right_in_inches * printing::kPointsPerInch);
  margins_in_points.top =
      base::ClampFloor(margin_top_in_inches * printing::kPointsPerInch);
  margins_in_points.bottom =
      base::ClampFloor(margin_bottom_in_inches * printing::kPointsPerInch);
  print_settings.SetCustomMargins(margins_in_points);

  double paper_width_in_inches =
      paper_width.value_or(printing::kLetterWidthInch);
  double paper_height_in_inches =
      paper_height.value_or(printing::kLetterHeightInch);
  if (paper_width_in_inches <= 0)
    return "paper width is zero or negative";

  if (paper_height_in_inches <= 0)
    return "paper height is zero or negative";

  gfx::Size paper_size_in_points = gfx::ToCeiledSize(
      gfx::SizeF(paper_width_in_inches * printing::kPointsPerInch,
                 paper_height_in_inches * printing::kPointsPerInch));
  gfx::Rect printable_area_device_units(paper_size_in_points);
  print_settings.SetPrinterPrintableArea(paper_size_in_points,
                                         printable_area_device_units, true);

  auto print_pages_params = printing::mojom::PrintPagesParams::New();
  print_pages_params->params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(print_settings,
                                          print_pages_params->params.get());
  print_pages_params->params->print_to_pdf = true;
  print_pages_params->params->document_cookie =
      printing::PrintSettings::NewCookie();
  print_pages_params->params->header_template =
      base::UTF8ToUTF16(header_template.value_or(""));
  print_pages_params->params->footer_template =
      base::UTF8ToUTF16(footer_template.value_or(""));
  print_pages_params->params->prefer_css_page_size =
      prefer_css_page_size.value_or(false);
  print_pages_params->params->generate_tagged_pdf = generate_tagged_pdf;
  using GenerateDocumentOutline = printing::mojom::GenerateDocumentOutline;
  print_pages_params->params->generate_document_outline =
      generate_document_outline.value_or(false)
          ? GenerateDocumentOutline::kFromAccessibilityTreeHeaders
          : GenerateDocumentOutline::kNone;

  CHECK(!print_pages_params->params->page_size.IsEmpty())
      << print_pages_params->params->page_size.ToString();

  if (print_pages_params->params->printable_area.IsEmpty()) {
    return "invalid print parameters: printable area is empty";
  }

  if (print_pages_params->params->content_size.IsEmpty()) {
    return "invalid print parameters: content area is empty";
  }

  if (!printing::PrintMsgPrintParamsIsValid(*print_pages_params->params)) {
    return "invalid print parameters";
  }

  return print_pages_params;
}

}  // namespace print_to_pdf
