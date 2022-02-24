// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/browser/print_manager_utils.h"
#include "printing/print_settings.h"
#include "printing/units.h"
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

absl::variant<printing::PageRanges, PageRangeError> TextPageRangesToPageRanges(
    base::StringPiece page_range_text,
    bool ignore_invalid_page_ranges,
    uint32_t expected_page_count) {
  printing::PageRanges page_ranges;
  for (const auto& range_string :
       base::SplitStringPiece(page_range_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    printing::PageRange range;
    if (range_string.find("-") == base::StringPiece::npos) {
      if (!base::StringToUint(range_string, &range.from))
        return PageRangeError::SYNTAX_ERROR;
      range.to = range.from;
    } else if (range_string == "-") {
      range.from = 1;
      range.to = expected_page_count;
    } else if (base::StartsWith(range_string, "-")) {
      range.from = 1;
      if (!base::StringToUint(range_string.substr(1), &range.to))
        return PageRangeError::SYNTAX_ERROR;
    } else if (base::EndsWith(range_string, "-")) {
      range.to = expected_page_count;
      if (!base::StringToUint(range_string.substr(0, range_string.length() - 1),
                              &range.from)) {
        return PageRangeError::SYNTAX_ERROR;
      }
    } else {
      auto tokens = base::SplitStringPiece(
          range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() != 2 || !base::StringToUint(tokens[0], &range.from) ||
          !base::StringToUint(tokens[1], &range.to)) {
        return PageRangeError::SYNTAX_ERROR;
      }
    }

    if (range.from < 1 || range.from > range.to) {
      if (!ignore_invalid_page_ranges)
        return PageRangeError::SYNTAX_ERROR;
      continue;
    }
    if (range.from > expected_page_count) {
      if (!ignore_invalid_page_ranges)
        return PageRangeError::LIMIT_ERROR;
      continue;
    }

    if (range.to > expected_page_count)
      range.to = expected_page_count;

    // Page numbers are 1-based in the dictionary.
    // Page numbers are 0-based for the print settings.
    range.from--;
    range.to--;
    page_ranges.push_back(range);
  }
  return page_ranges;
}

absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
GetPrintPagesParams(const GURL& page_url,
                    absl::optional<bool> landscape,
                    absl::optional<bool> display_header_footer,
                    absl::optional<bool> print_background,
                    absl::optional<double> scale,
                    absl::optional<double> paper_width,
                    absl::optional<double> paper_height,
                    absl::optional<double> margin_top,
                    absl::optional<double> margin_bottom,
                    absl::optional<double> margin_left,
                    absl::optional<double> margin_right,
                    absl::optional<std::string> header_template,
                    absl::optional<std::string> footer_template,
                    absl::optional<bool> prefer_css_page_size) {
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
  margins_in_points.left = margin_left_in_inches * printing::kPointsPerInch;
  margins_in_points.right = margin_right_in_inches * printing::kPointsPerInch;
  margins_in_points.top = margin_top_in_inches * printing::kPointsPerInch;
  margins_in_points.bottom = margin_bottom_in_inches * printing::kPointsPerInch;
  print_settings.SetCustomMargins(margins_in_points);

  double paper_width_in_inches =
      paper_width.value_or(printing::kLetterWidthInch);
  double paper_height_in_inches =
      paper_height.value_or(printing::kLetterHeightInch);
  if (paper_width_in_inches <= 0)
    return "paper width is zero or negative";

  if (paper_height_in_inches <= 0)
    return "paper height is zero or negative";

  gfx::Size paper_size_in_points(
      paper_width_in_inches * printing::kPointsPerInch,
      paper_height_in_inches * printing::kPointsPerInch);
  gfx::Rect printable_area_device_units(paper_size_in_points);
  print_settings.SetPrinterPrintableArea(paper_size_in_points,
                                         printable_area_device_units, true);

  auto print_pages_params = printing::mojom::PrintPagesParams::New();
  print_pages_params->params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(print_settings,
                                          print_pages_params->params.get());
  print_pages_params->params->document_cookie =
      printing::PrintSettings::NewCookie();
  print_pages_params->params->header_template =
      base::UTF8ToUTF16(header_template.value_or(""));
  print_pages_params->params->footer_template =
      base::UTF8ToUTF16(footer_template.value_or(""));
  print_pages_params->params->prefer_css_page_size =
      prefer_css_page_size.value_or(false);

  return print_pages_params;
}

}  // namespace print_to_pdf
