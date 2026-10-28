// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_heuristic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/timer/elapsed_timer.h"
#include "components/pdf/renderer/pdf_accessibility_text_utils.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_features.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

// How a list marker appears in its text run.
enum class ListMarkerForm {
  // Not a list marker. This includes marker-only runs without item text after
  // them on the same line, such as decorative rows of bullets.
  kNone,
  // A marker-only run (e.g. "•") followed by a separate item text run on the
  // same visual line.
  kStandalone,
  // A single run combining a marker and item text (e.g. "• Item").
  kCombined,
};

// Classifies a list marker and records its properties.
struct ListMarkerKind {
  ListMarkerForm form = ListMarkerForm::kNone;
  // The number of a decimal ordered list marker; unset for unordered markers.
  std::optional<int> ordered_value;
};

namespace {

// Histogram range parameters for recording heading-to-body font size ratios
// (ratio * 100).
constexpr int kHeadingToBodySizeRatioMin = 100;
constexpr int kHeadingToBodySizeRatioMax = 500;
// 42 buckets from 100 to 500 gives a step size of 10 units
// ((500 - 100) / (42 - 2) = 10), resulting in 10-unit wide linear buckets:
constexpr size_t kHeadingToBodySizeRatioBucketCount = 42;

// Don't try to apply font size thresholds to automatically identify headings
// if the median font size is not at least this many points.
constexpr float kMinimumFontSize = 5.0f;

// Font sizes within this threshold of each other are considered equivalent for
// heading levels.
constexpr float kFontSizeWiggleRoom = 1.0f;

// Don't try to apply paragraph break thresholds to automatically identify
// paragraph breaks if the median line break is not at least this many points.
constexpr float kMinimumLineSpacing = 5.0f;

// Ratio between the font size of one text run and the median on the page
// for that text run to be considered to be a heading instead of normal text.
constexpr float kHeadingFontSizeRatio = 1.2f;

// Ratio between the largest heading candidate font size and the median font
// size on the page for it to be considered an H1 instead of H2.
constexpr float kH1MinFontSizeRatio = 1.7f;

// Character count threshold below which a page is considered sparse (such as a
// cover page or slide) when selecting the median font size.
constexpr uint64_t kSparsePageCharCountThreshold = 250;

// On a sparse page, the second most frequent font size must account for at
// least this share of the page's characters to be chosen as the median.
constexpr float kSparsePageMinSecondFontSizeRatio = 0.1f;

// On a text-dense page, the second most frequent font size must have at least
// this many characters relative to the most frequent size to be chosen as the
// median.
constexpr float kSimilarFontSizeCharCountRatio = 0.4f;

// Ratio between the line spacing between two lines and the median on the
// page for that line spacing to be considered a paragraph break.
constexpr float kParagraphLineSpacingRatio = 1.2f;

// The default heading level used when the run is determined to be a heading.
constexpr int kDefaultHeadingLevel = 2;

// The largest heading level used when the run is determined to be a heading due
// to a combination of its font size and other styling, instead of just size.
constexpr int kLargestStyledHeadingLevel = 3;

// The largest heading level allowed (corresponds to <h1>).
constexpr int kLargestHeadingLevel = 1;

// The smallest heading level allowed (corresponds to <h6>).
constexpr int kSmallestHeadingLevel = 6;

// Font weight for semi-bold text. Used to determine if the run could be a
// heading.
constexpr int kSemiBoldWeight = 600;

// Minimum number of uppercase letters required for a standalone text run to be
// considered all-uppercase, filtering out single-letter labels or codes (e.g.
// "(A)", "P", "8B", "R1").
constexpr size_t kMinUppercaseLetterCount = 2;

// Min page height needed to label headers and footers. Page bounds are
// reported in CSS pixels at 96 DPI, so this is just over an inch. A page
// shorter than this is too small for a margin band to mean anything.
constexpr int kMinHeaderFooterPageHeight = 100;

// Margin ratios for header and footer margins. Expressed as a fraction of the
// vertical height of the page.
constexpr float kHeaderMarginRatio = 0.10f;
// Text that seems like a page number gets more leniency in how far up the page
// it can go, whereas text that doesn't seem like page numbers has a smaller
// allowable range on the page.
constexpr float kPageNumberFooterMarginRatio = 0.90f;
constexpr float kNonPageNumberFooterMarginRatio = 0.95f;

// Initial margin ratio for the left and right side margins, expressed as a
// fraction of the page width, before clamping outward to any body text that
// extends closer to the page edges.
constexpr float kSideMarginRatio = 0.085f;

// Largest width, as a fraction of the page width, allowed for text in the
// margins to be considered a page number.
constexpr float kMaxPageNumberWidthRatio = 0.30f;

// Maximum page area and height ratios for a form graphic to be treated as an
// embedded figure rather than a full-page background or sidebar template.
constexpr float kMaxGraphicPageAreaRatio = 0.8f;
constexpr float kMaxGraphicPageHeightRatio = 0.85f;

// Minimum width and height for an unlabeled image to be included in the
// accessibility tree. Smaller images are typically 1-pixel scanline slices or
// decorative spacer artifacts.
constexpr float kMinImageDimension = 5.0f;

// Font size ratio below which a run counts as substantially smaller than the
// text beside it on the same line, such as a superscript.
constexpr float kSmallTextFontSizeRatio = 0.85f;

// Maximum number of characters in a text run for it to be considered a
// superscript or subscript. Superscripts and subscripts are short markers or
// expressions (e.g., footnote numbers, citations, or chemical indices), not
// full phrases or sentences.
constexpr size_t kMaxSuperOrSubScriptCharCount = 8;

// Vertical tolerance (as a fraction of the host run's height) allowed when
// checking whether a superscript or subscript run stays near the host run's
// vertical span.
constexpr float kSuperOrSubScriptVerticalToleranceRatio = 0.10f;

// Tolerance used for comparisons, so that values that differ only by floating
// point imprecision compare as equal.
constexpr float kComparisonEpsilon = 0.01f;

// As defined in ISO 32000-1:2008, section 9.6.4: "Font Subsets".
// For a font subset, the PostScript name shall begin with 6 uppercase letters
// followed by a '+' tag applied by the subsetting application.
constexpr size_t kFontSubsetTagLength = 6;

// Horizontal outdent tolerance (in page pixels) before a line is considered
// outside the current list.
constexpr float kListIndentThreshold = 5.0f;

// Largest indent, in multiples of the item text's font size, that opens a
// nested list. A marker further right than this (e.g. the top of the next
// column in a multi-column layout) starts a new list instead.
constexpr float kMaxListNestingIndentEm = 4.0f;

// Maximum lines of item text to scan past when looking for the next ordered
// marker that confirms a new ordered list.
constexpr size_t kMaxOrderedListLookaheadLines = 15;

// Maximum digits in a decimal ordered list marker, so years (e.g. "2023.") are
// not mistaken for markers.
constexpr size_t kMaxOrderedMarkerDigits = 3;

// Delimiters that may follow the number in a decimal ordered list marker.
// TODO(crbug.com/525509231): Support delimiters used in other languages, such
// as ordinal indicators (e.g. "2º").
constexpr auto kOrderedMarkerDelimiters =
    std::to_array<std::string_view>({".", ")", ".)"});

// Markers that only count when they open or continue a nested level, because
// at the top level they usually aren't bullets (e.g. "–" as punctuation).
// LaTeX uses these for itemize levels 2-4.
constexpr auto kNestedOnlyListMarkers = std::to_array<std::string_view>({
    "\xE2\x80\x93",  // – U+2013 (en dash)
    "\xE2\x88\x97",  // ∗ U+2217 (asterisk operator)
    "\xC2\xB7",      // · U+00B7 (middle dot)
});

// Lowercase font name substrings for Word's nested-only list markers.
constexpr std::string_view kCourierFontSubstring = "courier";
constexpr std::string_view kWingdingsFontSubstring = "wingdings";

// Code points used only as bullets, covering the defaults of common authoring
// tools. Characters with other common uses, such as '-', '*', 'o', or dashes,
// are excluded to avoid false positives (e.g. "-" for an empty table cell or
// a minus sign).
constexpr auto kUnorderedListMarkers = std::to_array<std::string_view>({
    "\xE2\x80\xA2",  // • U+2022
    "\xE2\x80\xA3",  // ‣ U+2023
    "\xE2\x81\x83",  // ⁃ U+2043
    "\xE2\x97\xA6",  // ◦ U+25E6
    "\xE2\x96\xAA",  // ▪ U+25AA
    "\xE2\x97\x8B",  // ○ U+25CB
    "\xE2\x97\x8F",  // ● U+25CF
    "\xE2\x96\xA0",  // ■ U+25A0
    "\xEF\x82\xB7",  // U+F0B7 (Symbol font bullet)
    "\xEF\x82\xA7",  // U+F0A7 (Wingdings square bullet)
});

// ▪ U+25AA, the standard bullet for Wingdings markers.
constexpr uint32_t kSquareBulletCodePoint = 0x25AA;

// Maps private-use bullets that PDFium produces for common authoring fonts in
// `kUnorderedListMarkers` to their standard Unicode equivalents so screen
// readers announce them properly. All other markers in `kUnorderedListMarkers`
// are already valid standard Unicode characters and do not need remapping.
constexpr auto kPrivateUseBulletMap =
    base::MakeFixedFlatMap<uint32_t, uint32_t>({
        // Wingdings square bullet -> ▪ U+25AA
        {0xF0A7, kSquareBulletCodePoint},
        // Symbol font bullet -> • U+2022
        {0xF0B7, 0x2022},
    });

enum class HeaderFooterRole {
  kNone,
  kHeader,
  kFooter,
};

// How closely a text run resembles a page number. Note that this does not
// handle Roman numerals.
enum class PageNumberKind {
  // Does not look like a page number.
  kNone,
  // Narrow text containing at least one digit, e.g. "Page 3 of 10".
  kNarrowWithDigit,
  // Digits only, e.g. "42".
  kPureNumber,
};

// Where a list marker goes relative to the active list levels.
enum class ListPlacementKind {
  // Starts a new top-level list.
  kNewList,
  // Starts a nested list inside the innermost level's last item.
  kNested,
  // Joins an active level, popping any deeper levels.
  kExistingLevel,
};

// Result of `GetListPlacement()`.
struct ListPlacement {
  ListPlacementKind kind;
  // For `kNested` or `kExistingLevel`, the index of the level the item joins.
  size_t level_index = 0;
};

// Returns whether `heading_level` is in bounds, i.e. whether it corresponds to
// one of <h1> through <h6>.
bool IsValidHeadingLevel(int heading_level) {
  return heading_level >= kLargestHeadingLevel &&
         heading_level <= kSmallestHeadingLevel;
}

// Helper to determine whether two vertical spans overlap enough to be on the
// same line.
bool DoBoundsOverlapOnLine(float top1,
                           float height1,
                           float top2,
                           float height2) {
  if (height1 == 0.0f || height2 == 0.0f) {
    return false;
  }

  float clamped_top = std::max(top1, top2);
  float clamped_bottom = std::min(top1 + height1, top2 + height2);
  if (clamped_bottom < clamped_top) {
    return false;
  }

  // See if it falls within the line (within the threshold). Measure against
  // the shorter box when the enhancements flag is enabled so a smaller raised
  // or lowered run still counts as sharing the line with its taller neighbor.
  float reference_height =
      features::IsPdfAccessibilityHeuristicEnhancementsEnabled()
          ? std::min(height1, height2)
          : height2;
  float coverage = (clamped_bottom - clamped_top) / reference_height;
  constexpr float kLineCoverageThreshold = 0.25f;
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    // Include an exact 25% overlap, with epsilon tolerance for floating-point
    // rounding from PDF point-to-pixel scaling.
    return (coverage + kComparisonEpsilon) >= kLineCoverageThreshold;
  }
  return coverage > kLineCoverageThreshold;
}

// This class is used as part of our heuristic to determine which text runs live
// on the same "line".  As we process runs, we keep a weighted average of the
// top and bottom coordinates of the line, and if a new run falls within that
// range (within a threshold) it is considered part of the line.
class LineHelper {
 public:
  explicit LineHelper(
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs)
      : text_runs_(text_runs) {
    StartNewLine(0);
  }

  LineHelper(const LineHelper&) = delete;
  LineHelper& operator=(const LineHelper&) = delete;

  void StartNewLine(size_t current_index) {
    DCHECK(current_index == 0 || current_index < text_runs_->size());
    start_index_ = current_index;
    accumulated_weight_top_ = 0.0f;
    accumulated_weight_bottom_ = 0.0f;
    accumulated_width_ = 0.0f;
  }

  void ProcessNextRun(size_t run_index) {
    DCHECK_LT(run_index, text_runs_->size());
    RemoveOldRunsUpTo(run_index);
    AddRun((*text_runs_)[run_index].bounds);
  }

  bool IsRunOnSameLine(size_t run_index) const {
    DCHECK_LT(run_index, text_runs_->size());

    // Calculate new top/bottom bounds for our line.
    if (accumulated_width_ == 0.0f) {
      return false;
    }

    float line_top = accumulated_weight_top_ / accumulated_width_;
    float line_bottom = accumulated_weight_bottom_ / accumulated_width_;
    float line_height = line_bottom - line_top;

    // Look at the next run, and determine how much it overlaps the line.
    const auto& run_bounds = (*text_runs_)[run_index].bounds;
    return DoBoundsOverlapOnLine(line_top, line_height, run_bounds.y(),
                                 run_bounds.height());
  }

  // Returns whether the run at `run_index` is on the current line and
  // continues in reading order from the preceding run.
  bool ContinuesLineInReadingOrder(size_t run_index) const {
    if (!IsRunOnSameLine(run_index)) {
      return false;
    }

    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      CHECK_GT(run_index, 0u);
      const auto& run = (*text_runs_)[run_index];
      const auto& prev_run = (*text_runs_)[run_index - 1];

      // Reject backward-X jumps between left-to-right runs on the same
      // horizontal band (e.g. across columns or from later authoring layers)
      // from continuing the line.
      if (IsLeftToRightRun(prev_run) && IsLeftToRightRun(run) &&
          run.bounds.right() <= prev_run.bounds.x() + kComparisonEpsilon) {
        return false;
      }
    }

    return true;
  }

 private:
  void AddRun(const gfx::RectF& run_bounds) {
    float run_width = std::abs(run_bounds.width());
    accumulated_width_ += run_width;
    accumulated_weight_top_ += run_bounds.y() * run_width;
    accumulated_weight_bottom_ +=
        (run_bounds.y() + run_bounds.height()) * run_width;
  }

  void RemoveRun(const gfx::RectF& run_bounds) {
    float run_width = std::abs(run_bounds.width());
    accumulated_width_ -= run_width;
    accumulated_weight_top_ -= run_bounds.y() * run_width;
    accumulated_weight_bottom_ -=
        (run_bounds.y() + run_bounds.height()) * run_width;
  }

  void RemoveOldRunsUpTo(size_t stop_index) {
    // Remove older runs from the weighted average if we've exceeded the
    // threshold distance from them. We remove them to prevent e.g. drop-caps
    // from unduly influencing future lines.
    constexpr float kBoxRemoveWidthThreshold = 3.0f;
    while (start_index_ < stop_index &&
           accumulated_width_ > (*text_runs_)[start_index_].bounds.width() *
                                    kBoxRemoveWidthThreshold) {
      const auto& old_bounds = (*text_runs_)[start_index_].bounds;
      RemoveRun(old_bounds);
      start_index_++;
    }
  }

  const raw_ref<const std::vector<chrome_pdf::AccessibilityTextRunInfo>>
      text_runs_;
  size_t start_index_;
  float accumulated_weight_top_;
  float accumulated_weight_bottom_;
  float accumulated_width_;
};

template <typename T>
bool IsObjectInTextRun(base::span<const T> objects,
                       uint32_t object_index,
                       size_t text_run_index) {
  return (object_index < objects.size() &&
          objects[object_index].text_run_index <= text_run_index);
}

template <typename T>
bool IsObjectWithRangeInTextRun(const std::vector<T>& objects,
                                uint32_t object_index,
                                size_t text_run_index) {
  return (object_index < objects.size() &&
          objects[object_index].text_range.index <= text_run_index);
}

size_t NormalizeTextRunIndex(uint32_t object_end_text_run_index,
                             size_t current_text_run_index) {
  return std::max<size_t>(
      object_end_text_run_index,
      current_text_run_index ? current_text_run_index - 1 : 0);
}

// Returns the number of uppercase letters in `chars` if `chars` contains no
// lowercase letters, or 0 otherwise.
size_t CountAllUppercaseLetters(
    base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  size_t uppercase_count = 0;
  for (const auto& char_info : chars) {
    UChar32 c = static_cast<UChar32>(char_info.unicode_character);
    if (u_islower(c)) {
      return 0;
    }
    if (u_isupper(c)) {
      ++uppercase_count;
    }
  }
  return uppercase_count;
}

bool ContainsAlphanumeric(std::string_view text) {
  for (size_t i = 0; i < text.size(); ++i) {
    // `ReadUnicodeCharacter()` advances `i` to the last byte of the code point
    // it decoded, so the loop's `++i` lands on the start of the next code
    // point. It advances `i` even when it fails, so malformed input is skipped
    // rather than decoded again.
    base_icu::UChar32 code_point;
    if (base::ReadUnicodeCharacter(text, &i, &code_point) &&
        u_isalnum(code_point)) {
      return true;
    }
  }
  return false;
}

bool IsUnicodeDigit(std::string_view text, size_t* i) {
  base_icu::UChar32 code_point;
  // `ReadUnicodeCharacter()` advances `i` to the last byte of the code point
  // it decoded, so the loop's `++i` lands on the start of the next code
  // point. It advances `i` even when it fails, so malformed input is skipped
  // rather than decoded again.
  return base::ReadUnicodeCharacter(text, i, &code_point) &&
         u_isdigit(code_point);
}

// Returns how `text` qualifies as a page number. Leading and trailing
// whitespace is ignored.
PageNumberKind ClassifyPageNumber(std::string_view text,
                                  float run_width,
                                  float max_page_number_width) {
  const std::string_view trimmed =
      base::TrimWhitespaceASCII(text, base::TRIM_ALL);
  if (trimmed.empty()) {
    return PageNumberKind::kNone;
  }

  // Decode once, collecting both facts the classification needs: whether any
  // digit is present, and whether anything other than digits is.
  bool has_digit = false;
  bool has_non_digit = false;
  for (size_t i = 0; i < trimmed.size(); ++i) {
    if (IsUnicodeDigit(trimmed, &i)) {
      has_digit = true;
    } else {
      has_non_digit = true;
    }
    // Both answers are known once one of each has been seen.
    if (has_digit && has_non_digit) {
      break;
    }
  }

  if (!has_digit) {
    return PageNumberKind::kNone;
  }
  if (!has_non_digit) {
    return PageNumberKind::kPureNumber;
  }

  // Narrow text relative to the page width containing at least one digit.
  // Width is measured rather than character count because the rendered
  // footprint of a page number is roughly the same in any language, while the
  // number of characters needed to express it is not.
  bool is_narrow = run_width > 0.0f && run_width <= max_page_number_width;
  return is_narrow ? PageNumberKind::kNarrowWithDigit : PageNumberKind::kNone;
}

// Returns whether a font name indicates a bold, semi-bold, black, or heavy
// heading font style based on delimited font style patterns (e.g. "-bold").
bool IsHeadingFontName(std::string_view font_name) {
  static constexpr std::string_view kHeadingPatterns[] = {
      "-bold",      ",bold",      " bold",     "+bold",      "-semibold",
      ",semibold",  " semibold",  "+semibold", "-demi",      ",demi",
      " demi",      "+demi",      "-black",    ",black",     " black",
      "+black",     "-blk",       ",blk",      " blk",       "+blk",
      "-heavy",     ",heavy",     " heavy",    "+heavy",     "-extrabld",
      ",extrabld",  " extrabld",  "+extrabld", "-ultrabold", ",ultrabold",
      " ultrabold", "+ultrabold",
  };

  std::string lower_font_name = base::ToLowerASCII(font_name);
  for (std::string_view pattern : kHeadingPatterns) {
    if (lower_font_name.contains(pattern)) {
      return true;
    }
  }

  return false;
}

// A distinct font size on the page and the total number of characters drawn at
// that size.
struct FontSizeCharCount {
  float font_size = 0.0f;
  uint32_t char_count = 0;
};

// `font_sizes` holds one entry per text run on the page. Returns the median
// font size of those entries.
float SelectUnweightedMedianFontSize(std::vector<float> font_sizes) {
  CHECK(!features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  CHECK(!font_sizes.empty());
  std::ranges::sort(font_sizes);
  return font_sizes[font_sizes.size() / 2];
}

// `font_size_char_counts` maps each distinct font size on the page (sorted
// ascending) to its total character count. Returns the dominant font size on
// the page weighted by character count.
float SelectMedianFontSize(
    const std::map<float, uint32_t>& font_size_char_counts) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());

  // Track the top two font sizes by character count (`top1` and `top2`), since
  // a title on a sparse page or a table/footnote block on a dense page can
  // outnumber the body text and push the body font size into `top2`.
  uint64_t total_chars = 0;
  FontSizeCharCount top1;
  FontSizeCharCount top2;
  for (const auto& [font_size, char_count] : font_size_char_counts) {
    total_chars += char_count;
    if (char_count >= top1.char_count) {
      top2 = top1;
      top1 = {.font_size = font_size, .char_count = char_count};
    } else if (char_count >= top2.char_count) {
      top2 = {.font_size = font_size, .char_count = char_count};
    }
  }

  // Only consider `top2` if it has enough characters to represent body text
  // rather than a short heading, footnote, or superscript.
  const bool is_sparse_page = total_chars < kSparsePageCharCountThreshold;
  const float min_top2_chars =
      is_sparse_page ? total_chars * kSparsePageMinSecondFontSizeRatio
                     : top1.char_count * kSimilarFontSizeCharCountRatio;
  if (top2.char_count >= min_top2_chars) {
    const auto [smaller_size, larger_size] =
        std::minmax(top1.font_size, top2.font_size);
    // Never select a font size at or below `kMinimumFontSize` when a larger
    // dominant size is present (e.g. leader dots or punctuation runs).
    if (smaller_size <= kMinimumFontSize) {
      return larger_size;
    }
    // On sparse pages, prefer the smaller size so that a dominant title still
    // exceeds the median font size.
    if (is_sparse_page) {
      return smaller_size;
    }
    // On text-dense pages, prefer the larger size (e.g. body text over a table
    // or footnotes) when it falls in the non-H1 heading range relative to the
    // smaller size, so that the body text is not misclassified as headings.
    if (larger_size > smaller_size * kHeadingFontSizeRatio &&
        larger_size < smaller_size * kH1MinFontSizeRatio) {
      return larger_size;
    }
  }

  // Otherwise, default to the single most frequent font size on the page.
  return top1.font_size;
}

void ComputeFontSizes(std::vector<float> font_sizes,
                      const std::map<float, uint32_t>& font_size_char_counts,
                      float* out_heading_font_size_threshold,
                      float* out_median_font_size,
                      std::map<float, int>* out_heading_font_size_mapping) {
  if (font_sizes.size() <= 2) {
    return;
  }

  const float median =
      features::IsPdfAccessibilityHeuristicEnhancementsEnabled()
          ? SelectMedianFontSize(font_size_char_counts)
          : SelectUnweightedMedianFontSize(std::move(font_sizes));
  if (median <= kMinimumFontSize) {
    return;
  }

  *out_median_font_size = median;
  *out_heading_font_size_threshold =
      *out_median_font_size * kHeadingFontSizeRatio;

  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    return;
  }

  CHECK(out_heading_font_size_mapping->empty());
  // Start at heading level 1 only if the font size is significantly
  // larger than the median.
  float current_cluster_font_size = font_size_char_counts.rbegin()->first;
  bool is_much_larger = current_cluster_font_size >=
                        (*out_median_font_size * kH1MinFontSizeRatio);
  int current_level = is_much_larger ? 1 : 2;
  float min_mapping_font_size = *out_median_font_size;
  // Iterate from the largest font size down to the median font size. The
  // largest font size is compared to itself in the first iteration of the
  // loop so that it's set as the first level.
  for (const auto& [size, _] : std::views::reverse(font_size_char_counts)) {
    if (size < min_mapping_font_size) {
      break;
    }
    // If the current cluster size and the new size are different enough,
    // update the heading level. Otherwise, maintain the current cluster.
    if (current_cluster_font_size - size > kFontSizeWiggleRoom) {
      current_cluster_font_size = size;
      if (current_level < kSmallestHeadingLevel) {
        current_level++;
      }
    }
    // Once the normal heading size threshold is reached, start at level
    // `kMaxStyledHeadingLevel` and increment from there so that no text
    // of size < heading threshold is a heading level h1 or h2.
    if (size < *out_heading_font_size_threshold &&
        current_level < kLargestStyledHeadingLevel) {
      current_level = kLargestStyledHeadingLevel;
    }
    (*out_heading_font_size_mapping)[size] = current_level;
  }
}

float ComputeLineSpacings(std::vector<float> line_spacings) {
  if (line_spacings.size() <= 4) {
    return 0.0f;
  }

  std::ranges::sort(line_spacings);
  float median_line_spacing = line_spacings[line_spacings.size() / 2];
  if (median_line_spacing > kMinimumLineSpacing) {
    return median_line_spacing * kParagraphLineSpacingRatio;
  }
  return 0.0f;
}

std::optional<uint32_t> ComputeColors(
    const std::map<uint32_t, uint32_t>& all_color_char_counts) {
  if (all_color_char_counts.size() < 2) {
    return std::nullopt;
  }

  auto it = std::ranges::max_element(
      all_color_char_counts,
      [](const auto& a, const auto& b) { return a.second < b.second; });
  return it->first;
}

std::vector<chrome_pdf::AccessibilityFormGraphicInfo> FilterGraphics(
    const std::vector<chrome_pdf::AccessibilityFormGraphicInfo>& graphics,
    const gfx::RectF& page_bounds) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  const float max_area =
      page_bounds.size().GetArea() * kMaxGraphicPageAreaRatio;
  const float max_height = page_bounds.height() * kMaxGraphicPageHeightRatio;
  std::vector<chrome_pdf::AccessibilityFormGraphicInfo> filtered_graphics;
  for (const auto& graphic : graphics) {
    // Skip full-page background templates and tall header/footer forms.
    if (graphic.bounds.height() < max_height &&
        graphic.bounds.size().GetArea() < max_area) {
      filtered_graphics.push_back(graphic);
    }
  }
  return filtered_graphics;
}

std::vector<chrome_pdf::AccessibilityImageInfo> FilterImages(
    const std::vector<chrome_pdf::AccessibilityImageInfo>& images) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  std::vector<chrome_pdf::AccessibilityImageInfo> filtered_images;
  for (const auto& image : images) {
    // Keep images that have explicit alt text or meet the minimum dimensions,
    // skipping sub-5px scanline slices and decorative spacer artifacts.
    if (!image.alt_text.empty() ||
        (image.bounds.width() >= kMinImageDimension &&
         image.bounds.height() >= kMinImageDimension)) {
      filtered_images.push_back(image);
    }
  }
  return filtered_images;
}

HeuristicPageProperties ComputeHeuristicPageProperties(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityFormGraphicInfo>& graphics,
    const gfx::RectF& page_bounds) {
  const float max_page_number_width =
      page_bounds.width() * kMaxPageNumberWidthRatio;
  const float top_margin = page_bounds.height() * kHeaderMarginRatio;
  const float bottom_page_number_margin =
      page_bounds.height() * kPageNumberFooterMarginRatio;
  float left_margin = page_bounds.width() * kSideMarginRatio;
  float right_margin = page_bounds.width() - left_margin;

  std::vector<float> font_sizes;
  std::map<float, uint32_t> font_size_char_counts;
  std::vector<float> line_spacings;
  std::map<uint32_t, uint32_t> all_color_char_counts;

  uint32_t current_graphic_index = 0;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    // Skip text runs inside graphics so diagram labels do not skew page-level
    // body text metrics.
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
        IsObjectWithRangeInTextRun(graphics, current_graphic_index, i)) {
      const auto& range = graphics[current_graphic_index++].text_range;
      i = NormalizeTextRunIndex(range.index + range.count - 1, i);
      continue;
    }

    const auto& run = text_runs[i];
    font_sizes.push_back(run.style.font_size);
    font_size_char_counts[run.style.font_size] += run.len;
    // TODO(crbug.com/525508832): Use a color distance threshold to group
    // visually indistinguishable but non-identical colors together.
    all_color_char_counts[run.style.fill_color] += run.len;

    // Clamp side margins to the horizontal bounds of body runs that overlap the
    // central body area so that short body lines or narrow table columns on
    // pages with narrow margins are not mistaken for side-margin artifacts.
    // Exclude runs in the top and bottom margins so wide headers or footers do
    // not shift the side margins outward.
    if (run.bounds.right() > left_margin && run.bounds.x() < right_margin &&
        run.bounds.bottom() > top_margin &&
        run.bounds.y() < bottom_page_number_margin) {
      left_margin = std::min(left_margin, run.bounds.x());
      right_margin = std::max(right_margin, run.bounds.right());
    }

    if (i > 0) {
      const auto& cur = run.bounds;
      const auto& prev = text_runs[i - 1].bounds;
      if (cur.y() > prev.y() + prev.height() / 2) {
        line_spacings.push_back(cur.y() - prev.y());
      }
    }
  }

  HeuristicPageProperties page_properties;
  page_properties.page_height = page_bounds.height();
  page_properties.page_offset_y = page_bounds.y();
  page_properties.max_page_number_width = max_page_number_width;
  page_properties.top_margin = top_margin;
  page_properties.bottom_page_number_margin = bottom_page_number_margin;
  page_properties.bottom_non_page_number_margin =
      page_bounds.height() * kNonPageNumberFooterMarginRatio;
  page_properties.left_margin = left_margin;
  page_properties.right_margin = right_margin;
  ComputeFontSizes(std::move(font_sizes), font_size_char_counts,
                   &page_properties.heading_font_size_threshold,
                   &page_properties.median_font_size,
                   &page_properties.heading_font_size_mapping);
  page_properties.paragraph_spacing_threshold =
      ComputeLineSpacings(std::move(line_spacings));
  page_properties.body_text_color = ComputeColors(all_color_char_counts);

  return page_properties;
}

// Returns the hierarchical heading level (1 to 6) for a given font size,
// based on the mapping of unique heading font sizes. Returns 0 if not a
// heading.
int GetHeadingLevelFromSize(
    const std::map<float, int>& heading_font_size_mapping,
    float font_size) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  if (heading_font_size_mapping.empty()) {
    return 0;
  }

  // Find the first key in the map that is >= font_size.
  auto it = heading_font_size_mapping.lower_bound(font_size);
  float best_diff = kFontSizeWiggleRoom;
  int best_level = 0;

  // Check the mapped candidate font size that is greater than or equal to
  // `font_size` (the upper bound).
  if (it != heading_font_size_mapping.end()) {
    float diff = std::abs(it->first - font_size);
    if (diff <= best_diff) {
      best_diff = diff;
      best_level = it->second;
    }
  }

  // Check the mapped candidate font size that is strictly smaller than
  // `font_size` (the lower neighbor).
  if (it != heading_font_size_mapping.begin()) {
    auto prev_it = std::prev(it);
    float diff = std::abs(prev_it->first - font_size);
    if (diff <= best_diff) {
      best_diff = diff;
      best_level = prev_it->second;
    }
  }

  return best_level;
}

void RecordHeadingToBodySizeRatioHistogram(std::string_view name, int sample) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      name, kHeadingToBodySizeRatioMin, kHeadingToBodySizeRatioMax,
      kHeadingToBodySizeRatioBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

// Records the body-facing edge of each header (bottom) and footer (top) block
// as a percentage of the page height, matching the edges the margin checks
// use. Called once block roles are final, since later runs can promote or
// demote a block.
void RecordHeaderFooterLocationHistograms(
    base::span<const ui::AXNodeData* const> block_nodes,
    const HeuristicPageProperties& page_properties) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  if (page_properties.page_height <= 0) {
    return;
  }

  for (const ui::AXNodeData* block_node : block_nodes) {
    const gfx::RectF& bounds = block_node->relative_bounds.bounds;
    std::string_view histogram_name;
    float edge_y;
    switch (block_node->role) {
      case ax::mojom::Role::kSectionHeader:
        histogram_name = "Accessibility.PdfHeuristics.HeaderNormalizedLocation";
        edge_y = bounds.bottom();
        break;
      case ax::mojom::Role::kSectionFooter:
        histogram_name = "Accessibility.PdfHeuristics.FooterNormalizedLocation";
        edge_y = bounds.y();
        break;
      default:
        continue;
    }
    // Block bounds are in document coordinates.
    float page_edge_y = edge_y - page_properties.page_offset_y;
    base::UmaHistogramPercentage(
        histogram_name,
        static_cast<int>(
            std::round(page_edge_y * 100.0f / page_properties.page_height)));
  }
}

// Returns a span of AccessibilityCharInfo corresponding to the text run at
// `text_run_index`.
base::span<const chrome_pdf::AccessibilityCharInfo> GetTextRunChars(
    const PageLayoutData& layout,
    size_t text_run_index) {
  uint32_t start_index = layout.text_run_start_indices[text_run_index];
  uint32_t len = layout.text_runs[text_run_index].len;
  return base::span(layout.chars).subspan(start_index, len);
}

// Returns the text of `chars` as a UTF-8 string, with leading and trailing
// whitespace removed.
std::string GetTrimmedText(
    base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  std::string result;
  for (const auto& char_info : chars) {
    base::WriteUnicodeCharacter(
        static_cast<base_icu::UChar32>(char_info.unicode_character), &result);
  }
  return std::string(base::TrimWhitespaceASCII(result, base::TRIM_ALL));
}

bool AreRunsOnSameLine(const chrome_pdf::AccessibilityTextRunInfo& run1,
                       const chrome_pdf::AccessibilityTextRunInfo& run2) {
  return DoBoundsOverlapOnLine(run1.bounds.y(), run1.bounds.height(),
                               run2.bounds.y(), run2.bounds.height());
}

const chrome_pdf::AccessibilityTextRunInfo* GetRunBeforeIndex(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    size_t index) {
  return (index > 0 && index < text_runs.size()) ? &text_runs[index - 1]
                                                 : nullptr;
}

const chrome_pdf::AccessibilityTextRunInfo* GetRunAfterIndex(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    size_t index) {
  return (index + 1 < text_runs.size()) ? &text_runs[index + 1] : nullptr;
}

TextRunContext GetTextRunContext(const PageLayoutData& layout,
                                 size_t text_run_index) {
  return {
      .run = raw_ref(layout.text_runs[text_run_index]),
      .next_run = GetRunAfterIndex(layout.text_runs, text_run_index),
      .chars = GetTextRunChars(layout, text_run_index),
      .next_chars = text_run_index + 1 < layout.text_runs.size()
                        ? GetTextRunChars(layout, text_run_index + 1)
                        : base::span<const chrome_pdf::AccessibilityCharInfo>(),
  };
}

// Symbols conventionally used as footnote or reference markers. Unlike other
// punctuation, they carry content when set as a super or subscript.
bool IsFootnoteMarkerSymbol(base_icu::UChar32 code_point) {
  switch (code_point) {
    case '*':
    case 0x00A7:  // Section sign.
    case 0x00B6:  // Pilcrow sign.
    case 0x2020:  // Dagger.
    case 0x2021:  // Double dagger.
    case 0x2217:  // Asterisk.
      return true;
    default:
      return false;
  }
}

// Returns whether `chars` can read as a superscript or subscript. Superscripts
// and subscripts are short and carry letters, digits, or footnote marker
// symbols. A run of only separators, such as a comma between two words, is
// geometrically indistinguishable from a super or subscript but is not one.
bool CanBeSuperOrSubScriptText(
    base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  if (chars.size() > kMaxSuperOrSubScriptCharCount) {
    return false;
  }

  return std::ranges::any_of(chars, [](const auto& char_info) {
    auto code_point =
        static_cast<base_icu::UChar32>(char_info.unicode_character);
    return u_isalnum(code_point) || IsFootnoteMarkerSymbol(code_point);
  });
}

// Checks whether `candidate` is positioned and sized like a superscript or
// subscript relative to `host` on the same visual line.
ax::mojom::TextPosition ClassifySuperOrSubScriptRelativeToHost(
    const chrome_pdf::AccessibilityTextRunInfo& candidate,
    const chrome_pdf::AccessibilityTextRunInfo& host) {
  if (candidate.style.font_size <= 0.0f || host.style.font_size <= 0.0f ||
      candidate.bounds.height() <= 0.0f || host.bounds.height() <= 0.0f) {
    return ax::mojom::TextPosition::kNone;
  }

  // Must be substantially smaller in font size and on the same visual line as
  // the host run.
  if (candidate.style.font_size >=
          host.style.font_size * kSmallTextFontSizeRatio ||
      !AreRunsOnSameLine(candidate, host)) {
    return ax::mojom::TextPosition::kNone;
  }

  // Must be horizontally adjacent to the host run, so that smaller text in a
  // separate column, sidebar, or table cell on the same visual line is not
  // misclassified.
  const float horizontal_gap =
      std::max(candidate.bounds.x() - host.bounds.right(),
               host.bounds.x() - candidate.bounds.right());
  if (horizontal_gap > host.bounds.height()) {
    return ax::mojom::TextPosition::kNone;
  }

  const float candidate_top = candidate.bounds.y();
  const float candidate_bottom = candidate.bounds.bottom();
  const float candidate_mid = candidate.bounds.CenterPoint().y();

  const float host_top = host.bounds.y();
  const float host_bottom = host.bounds.bottom();
  const float host_mid = host.bounds.CenterPoint().y();
  const float tolerance =
      host.bounds.height() * kSuperOrSubScriptVerticalToleranceRatio;

  // Superscript: vertical center sits above the host's vertical center, and
  // bottom does not extend significantly past the host's vertical center.
  // Bounds come from glyph boxes, so a smaller run on the host's baseline that
  // lacks descenders while the host has a descender (e.g. 'p' or 'g') ends at
  // the shared baseline rather than in the upper half of the host's box, and is
  // not a superscript.
  if (candidate_mid < host_mid && candidate_bottom <= host_mid + tolerance) {
    return ax::mojom::TextPosition::kSuperscript;
  }

  // Subscript: vertical center sits below the host's vertical center, the top
  // does not rise significantly above the host's top, and the bottom drops past
  // the host's bottom. Bounds come from glyph boxes, so a run that merely sets
  // smaller type on the host's baseline, such as an acronym in small caps,
  // shares the host's bottom edge and is not a subscript.
  if (candidate_mid > host_mid && candidate_top >= host_top - tolerance &&
      candidate_bottom > host_bottom) {
    return ax::mojom::TextPosition::kSubscript;
  }

  return ax::mojom::TextPosition::kNone;
}

// Returns whether the run at `text_run_index` reads as a superscript or a
// subscript relative to the run beside it on the same visual line, or `kNone`.
// `chars` are the characters of that run.
ax::mojom::TextPosition GetTextPosition(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    size_t text_run_index,
    base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    return ax::mojom::TextPosition::kNone;
  }

  if (!CanBeSuperOrSubScriptText(chars)) {
    return ax::mojom::TextPosition::kNone;
  }

  const chrome_pdf::AccessibilityTextRunInfo& run = text_runs[text_run_index];

  // Check against the preceding run first (e.g., "x²", "H₂O", "word¹"), then
  // against the following run (e.g., "¹Footnote text" at the start of a line).
  const chrome_pdf::AccessibilityTextRunInfo* previous_run =
      GetRunBeforeIndex(text_runs, text_run_index);
  if (previous_run) {
    ax::mojom::TextPosition position =
        ClassifySuperOrSubScriptRelativeToHost(run, *previous_run);
    if (position != ax::mojom::TextPosition::kNone) {
      return position;
    }
  }

  const chrome_pdf::AccessibilityTextRunInfo* next_run =
      GetRunAfterIndex(text_runs, text_run_index);
  if (next_run) {
    ax::mojom::TextPosition position =
        ClassifySuperOrSubScriptRelativeToHost(run, *next_run);
    if (position != ax::mojom::TextPosition::kNone) {
      return position;
    }
  }

  return ax::mojom::TextPosition::kNone;
}

// Returns the header or footer role that `run_context.run` qualifies for, or
// `kNone` otherwise. A run must sit inside the top or bottom margin band,
// contain at least one alphanumeric character, and be rendered smaller than the
// page's median font size, since running headers and copyright notices are
// often set smaller than body text. Page numbers are exempt from the font size
// rule and are allowed a taller bottom margin band, because they are commonly
// set at body text size and placed higher up the page. `run_context.next_run`,
// the run following `run_context.run` or null at the end of the page, tells
// whether other text shares the same visual line, which separates a page number
// from a footnote marker or a section number.
//
// `out_page_number_kind` receives how the run reads as a page number, so that
// callers can order this against heading classification without classifying the
// run's text a second time.
HeaderFooterRole GetHeaderFooterRole(
    const TextRunContext& run_context,
    const HeuristicPageProperties& page_properties,
    PageNumberKind* out_page_number_kind) {
  *out_page_number_kind = PageNumberKind::kNone;
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled() ||
      page_properties.page_height < kMinHeaderFooterPageHeight) {
    return HeaderFooterRole::kNone;
  }

  // Must contain at least one alphanumeric character (letter or digit) to be a
  // header or footer. This filters out isolated bullets, decorative rules, or
  // symbols.
  std::string run_text = GetTrimmedText(run_context.chars);
  if (run_text.empty() || !ContainsAlphanumeric(run_text)) {
    return HeaderFooterRole::kNone;
  }

  const chrome_pdf::AccessibilityTextRunInfo& current_run = *run_context.run;
  const chrome_pdf::AccessibilityTextRunInfo* next_run = run_context.next_run;

  // Margin check: early return for text outside top/bottom margins.
  bool is_in_top_margin =
      current_run.bounds.bottom() <= page_properties.top_margin &&
      current_run.bounds.bottom() > 0.0f;

  PageNumberKind page_number_kind =
      ClassifyPageNumber(run_text, current_run.bounds.width(),
                         page_properties.max_page_number_width);
  *out_page_number_kind = page_number_kind;
  bool is_page_number = page_number_kind != PageNumberKind::kNone;
  float bottom_margin = is_page_number
                            ? page_properties.bottom_page_number_margin
                            : page_properties.bottom_non_page_number_margin;
  bool is_in_bottom_margin =
      current_run.bounds.y() >= bottom_margin &&
      current_run.bounds.y() < page_properties.page_height;
  if (!is_in_top_margin && !is_in_bottom_margin) {
    return HeaderFooterRole::kNone;
  }

  bool has_median_font_size = page_properties.median_font_size > 0;
  bool has_same_line_neighbor =
      next_run && AreRunsOnSameLine(current_run, *next_run);
  bool is_larger_than_body_text =
      has_median_font_size &&
      current_run.style.font_size > page_properties.median_font_size;
  bool is_lone_numeral = page_number_kind == PageNumberKind::kPureNumber &&
                         !has_same_line_neighbor;

  // Headers and footers are not usually larger than body text. A lone numeral
  // is exempt, since page numbers are sometimes set large, but one with text
  // beside it on the same line is a section number, such as the "1" of
  // "1 Introduction", which PDFium splits into its own run.
  if (is_larger_than_body_text && !is_lone_numeral) {
    return HeaderFooterRole::kNone;
  }

  // A bare digit in a margin is a page number, unless it is a superscript or
  // subscript marker relative to the text beside it on the same line, which
  // belongs to the body content, so leave it unclassified.
  if (page_number_kind == PageNumberKind::kPureNumber) {
    if (next_run &&
        ClassifySuperOrSubScriptRelativeToHost(current_run, *next_run) !=
            ax::mojom::TextPosition::kNone) {
      return HeaderFooterRole::kNone;
    }
    return is_in_top_margin ? HeaderFooterRole::kHeader
                            : HeaderFooterRole::kFooter;
  }

  if (has_median_font_size) {
    // Headers and footers (running heads, copyright notices) are usually set
    // smaller than body text, so text at body size here is more likely body
    // content spilling into the margin. Page numbers are exempt.
    bool is_strictly_smaller =
        current_run.style.font_size <
        (page_properties.median_font_size - kComparisonEpsilon);
    if (!is_strictly_smaller && !is_page_number) {
      return HeaderFooterRole::kNone;
    }
  }

  if (is_in_top_margin) {
    return HeaderFooterRole::kHeader;
  }
  // The margin check above returns early unless the run is in one of the two
  // margin bands.
  CHECK(is_in_bottom_margin);
  return HeaderFooterRole::kFooter;
}

// Returns the AX role that corresponds to `role`, or `std::nullopt` for
// `kNone` so that callers can continue with heading classification.
std::optional<ax::mojom::Role> GetAXRoleForHeaderFooterRole(
    HeaderFooterRole role) {
  switch (role) {
    case HeaderFooterRole::kHeader:
      return ax::mojom::Role::kSectionHeader;
    case HeaderFooterRole::kFooter:
      return ax::mojom::Role::kSectionFooter;
    case HeaderFooterRole::kNone:
      return std::nullopt;
  }
}

bool IsInSideMargin(const chrome_pdf::AccessibilityTextRunInfo& run,
                    const HeuristicPageProperties& page_properties) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  const bool in_left_margin =
      run.bounds.right() <= page_properties.left_margin &&
      run.bounds.right() > 0.0f;
  const bool in_right_margin = run.bounds.x() >= page_properties.right_margin;
  return (in_left_margin || in_right_margin) &&
         run.bounds.width() <= page_properties.max_page_number_width;
}

std::string_view GetFontWithoutSubset(std::string_view font_name) {
  // As defined in ISO 32000-1:2008, section 9.6.4: "Font Subsets".
  // For a font subset, the PostScript name shall begin with 6 uppercase letters
  // followed by a '+' tag applied by the subsetting application.
  const bool has_plus_tag = font_name.size() > kFontSubsetTagLength + 1 &&
                            font_name[kFontSubsetTagLength] == '+';
  if (has_plus_tag &&
      std::ranges::all_of(font_name.substr(0, kFontSubsetTagLength),
                          &base::IsAsciiUpper<char>)) {
    return font_name.substr(kFontSubsetTagLength + 1);
  }
  return font_name;
}

bool AreStylesAndFontsEquivalent(
    const chrome_pdf::AccessibilityTextStyleInfo& style1,
    const chrome_pdf::AccessibilityTextStyleInfo& style2) {
  return PdfAccessibilityTreeBuilder::AreStylesEquivalent(style1, style2) &&
         GetFontWithoutSubset(style1.font_name) ==
             GetFontWithoutSubset(style2.font_name) &&
         style1.fill_color == style2.fill_color;
}

HeadingClassifier GetHeadingClassifier(
    const TextRunContext& run_context,
    const HeuristicPageProperties& page_properties) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());

  const chrome_pdf::AccessibilityTextRunInfo& current_run = *run_context.run;
  const chrome_pdf::AccessibilityTextRunInfo* next_run = run_context.next_run;
  const chrome_pdf::AccessibilityTextStyleInfo& style = current_run.style;
  if (style.font_size < page_properties.median_font_size) {
    return HeadingClassifier::kNone;
  }

  // Any styled heading candidate must terminate its visual line (the next run
  // cannot be on the same line unless it has the exact same style).
  bool is_same_line = next_run && AreRunsOnSameLine(current_run, *next_run);
  bool is_same_style = next_run && AreStylesAndFontsEquivalent(
                                       current_run.style, next_run->style);
  if (is_same_line && !is_same_style) {
    return HeadingClassifier::kNone;
  }

  // Handle styled text that is the same size as the normal body text with more
  // caution so that stylized body text isn't mistaken as a heading.
  size_t uppercase_count = CountAllUppercaseLetters(run_context.chars);
  bool is_run_all_uppercase =
      uppercase_count >= kMinUppercaseLetterCount ||
      (uppercase_count > 0 && is_same_line &&
       CountAllUppercaseLetters(run_context.next_chars) > 0);
  if (style.font_size == page_properties.median_font_size) {
    // If this is the last run of the page, label this body text.
    if (!next_run) {
      return HeadingClassifier::kNone;
    }

    // Inline uppercase tokens followed by more text on the same line (e.g.
    // acronyms or labels like "A.1") are body text.
    if (uppercase_count > 0 && is_same_line) {
      return HeadingClassifier::kNone;
    }

    // Non-all-caps text continuing onto a new line with the same style is body
    // text. All caps text continuing onto a new line with the same style is
    // more likely a heading.
    if (!is_run_all_uppercase && !is_same_line && is_same_style) {
      return HeadingClassifier::kNone;
    }
  }

  // Check all-caps before bold styling because if a run has both, the all caps
  // classification should take precedence. If `is_run_all_uppercase` is true
  // here, then this must be either larger than the median font size, or the
  // next run must be on a different line. In either case, it's likely to be a
  // heading.
  if (is_run_all_uppercase) {
    return HeadingClassifier::kAllUppercase;
  }

  if (PdfAccessibilityTreeBuilder::IsBoldStyle(style)) {
    return HeadingClassifier::kBoldStyle;
  }

  // `IsBoldStyle()` above is only true for weight >= 700, but semi-bold text
  // runs can still be headings.
  if (PdfAccessibilityTreeBuilder::GetFontWeight(style) >= kSemiBoldWeight) {
    return HeadingClassifier::kSemiBoldWeight;
  }

  // Not every PDF specifies its /FontWeight or /StemV properly. If none of the
  // above cases apply, check the font name which will often include the word
  // "bold" or similar.
  if (IsHeadingFontName(style.font_name)) {
    return HeadingClassifier::kFontName;
  }

  // Text color differs from dominant body text color.
  if (page_properties.body_text_color.has_value() &&
      style.fill_color != *page_properties.body_text_color) {
    return HeadingClassifier::kTextColor;
  }

  return HeadingClassifier::kNone;
}

void PromoteNodeToHeading(ui::AXNodeData* block_node, int heading_level) {
  CHECK(IsValidHeadingLevel(heading_level));
  block_node->role = ax::mojom::Role::kHeading;
  block_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                              heading_level);
  block_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                 "h" + base::NumberToString(heading_level));
}

// Re-evaluates the header or footer role of `block_node` for a later run on
// the same visual line, since the first run alone may not identify the line.
void UpdateHeaderFooterRoleForSameLineRun(
    const TextRunContext& run_context,
    const HeuristicPageProperties& page_properties,
    ui::AXNodeData* block_node) {
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    return;
  }

  // Only the header or footer role matters here. The page number kind is not
  // needed, since heading classification already ran when the block was
  // created.
  PageNumberKind unused_page_number_kind;
  HeaderFooterRole role = GetHeaderFooterRole(run_context, page_properties,
                                              &unused_page_number_kind);

  // Block bounds are in document coordinates, so subtract the page offset to
  // compare against page-relative margins.
  float block_page_y =
      block_node->relative_bounds.bounds.y() - page_properties.page_offset_y;

  // Wide text that is not a page number reads as body content, so demote the
  // footer back to a paragraph. Also demote when a block that started in the
  // side margin (outside the bottom margin) continues onto a run outside the
  // side margin on the same visual line (e.g. a bullet or footnote marker).
  if (block_node->role == ax::mojom::Role::kSectionFooter &&
      role == HeaderFooterRole::kNone) {
    if (run_context.run->bounds.width() >
            page_properties.max_page_number_width ||
        (block_page_y < page_properties.bottom_page_number_margin &&
         !IsInSideMargin(*run_context.run, page_properties))) {
      block_node->role = ax::mojom::Role::kParagraph;
    }
    return;
  }

  if (block_node->role != ax::mojom::Role::kParagraph) {
    return;
  }

  // Require the block to have started in the matching margin, and for footers
  // to still be narrow, so body text that merely reaches into the margin is not
  // promoted. Block bounds are in document coordinates, so subtract the page
  // offset to compare against page-relative margins.
  float block_page_bottom = block_node->relative_bounds.bounds.bottom() -
                            page_properties.page_offset_y;
  bool is_header = role == HeaderFooterRole::kHeader &&
                   block_page_bottom <= page_properties.top_margin;
  bool is_footer = role == HeaderFooterRole::kFooter &&
                   block_page_y >= page_properties.bottom_page_number_margin &&
                   block_node->relative_bounds.bounds.width() <=
                       page_properties.max_page_number_width;

  if (is_header || is_footer) {
    // `role` is a header or footer here, so the role always has a value.
    block_node->role = GetAXRoleForHeaderFooterRole(role).value();
  }
}

// Returns whether to break the current block at the header, footer, or
// side-margin boundary between `current_run_context` and `next_run_context`, or
// `std::nullopt` when the transition says nothing about breaking. Demotes
// `block_node` back to a paragraph when a block already classified as a header
// or bottom footer turns out to be body content.
std::optional<bool> BreakAtHeaderFooterBoundary(
    const TextRunContext& current_run_context,
    const TextRunContext& next_run_context,
    const HeuristicPageProperties& page_properties,
    bool is_large_line_spacing_break,
    ui::AXNodeData* block_node) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());

  HeaderFooterRole current_role = HeaderFooterRole::kNone;
  if (block_node->role == ax::mojom::Role::kSectionHeader) {
    current_role = HeaderFooterRole::kHeader;
  } else if (block_node->role == ax::mojom::Role::kSectionFooter) {
    current_role = HeaderFooterRole::kFooter;
  }

  // Always break when transitioning into or out of the side margin so that
  // side-margin artifacts are isolated from body blocks.
  bool current_in_side_margin =
      current_role == HeaderFooterRole::kFooter &&
      IsInSideMargin(*current_run_context.run, page_properties);
  bool next_in_side_margin =
      IsInSideMargin(*current_run_context.next_run, page_properties);
  if (current_in_side_margin != next_in_side_margin) {
    return true;
  }

  // Consecutive runs in the side margin break only on a paragraph-sized gap.
  if (current_in_side_margin) {
    return is_large_line_spacing_break;
  }

  PageNumberKind next_page_number_kind = PageNumberKind::kNone;
  HeaderFooterRole next_role = GetHeaderFooterRole(
      next_run_context, page_properties, &next_page_number_kind);

  if (current_role != HeaderFooterRole::kNone) {
    // Without a large gap, the next run continues the current line or opens one
    // at ordinary paragraph spacing, so this block was really the first line of
    // body text, or of a multi-line footnote in a margin. Demote it back to a
    // paragraph so it keeps growing.
    if (next_role == HeaderFooterRole::kNone && !is_large_line_spacing_break) {
      block_node->role = ax::mojom::Role::kParagraph;
      return false;
    }
    // Otherwise break when transitioning away from the active header or footer
    // block.
    return current_role != next_role;
  }

  // A header should never be part of a preceding body paragraph.
  if (next_role == HeaderFooterRole::kHeader) {
    return true;
  }

  // Only break into a footer on a paragraph-sized gap, or when the footer is a
  // pure page number (digits only). Narrow text containing digits alongside
  // words or punctuation (kNarrowWithDigit, such as a trailing date or citation
  // at the end of a footnote) requires a paragraph-sized gap so it is not
  // severed from its paragraph.
  if (next_role == HeaderFooterRole::kFooter &&
      (is_large_line_spacing_break ||
       next_page_number_kind == PageNumberKind::kPureNumber)) {
    return true;
  }

  return std::nullopt;
}

bool BreakParagraphByLineSpacing(
    const chrome_pdf::AccessibilityTextRunInfo& current_run,
    const chrome_pdf::AccessibilityTextRunInfo& next_run,
    float paragraph_spacing_threshold) {
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
      IsLeftToRightRun(current_run) && IsLeftToRightRun(next_run)) {
    // `y` increases down the page, so this checks whether the next run sits
    // entirely above the current one, i.e. the stream moved back up the page
    // (e.g. a title emitted after body text). Break even when the jump is
    // smaller than the paragraph spacing threshold, so the out-of-order text
    // gets its own block that SortBlockNodesByVerticalBands() can reorder.
    if (next_run.bounds.bottom() <=
        current_run.bounds.y() + kComparisonEpsilon) {
      return true;
    }

    // A backward X jump on the same vertical line indicates text emitted out
    // of reading order across side-by-side frames, which breaks the paragraph.
    if (next_run.bounds.right() <=
            current_run.bounds.x() + kComparisonEpsilon &&
        AreRunsOnSameLine(current_run, next_run)) {
      return true;
    }
  }

  float line_spacing = std::abs(next_run.bounds.y() - current_run.bounds.y());
  if (paragraph_spacing_threshold > 0) {
    return line_spacing > paragraph_spacing_threshold;
  }

  // If there's no threshold, that means there weren't enough lines to compute
  // an accurate median, so compare against the line size instead.
  return line_spacing >
         kParagraphLineSpacingRatio * current_run.bounds.height();
}

// Returns whether a standalone marker run is followed by item text on the same
// visual line.
bool IsStandaloneMarkerFollowedByText(const TextRunContext& run_context) {
  // Standalone markers are single-character `kNone` runs, so the item
  // run's direction must be checked separately before comparing x-order.
  const bool are_runs_in_ltr_order =
      run_context.next_run && IsLeftToRightRun(*run_context.next_run) &&
      run_context.next_run->bounds.x() >= run_context.run->bounds.x();
  // `AreRunsOnSameLine(a, b)` measures vertical overlap relative to `b`.
  // Passing `run` second ensures the marker's smaller glyph box does not
  // prevent overlap.
  return are_runs_in_ltr_order &&
         AreRunsOnSameLine(*run_context.next_run, *run_context.run) &&
         ContainsAlphanumeric(GetTrimmedText(run_context.next_chars));
}

// Classifies `text` against a set of marker strings.
ListMarkerForm ClassifyUnorderedMarker(
    std::string_view text,
    base::span<const std::string_view> markers,
    const TextRunContext& run_context) {
  for (std::string_view marker : markers) {
    if (!text.starts_with(marker)) {
      continue;
    }
    // A run containing only a marker is a standalone marker if the item text
    // follows in the next run on the same line. Otherwise, it is not a list
    // marker (e.g. a marker used decoratively or followed by more markers).
    if (text.size() == marker.size()) {
      if (IsStandaloneMarkerFollowedByText(run_context)) {
        return ListMarkerForm::kStandalone;
      }
      return ListMarkerForm::kNone;
    }
    // If the run text is a list marker, followed by a space and then text,
    // it is a combined list marker and text run.
    if (base::IsAsciiWhitespace(text[marker.size()]) &&
        ContainsAlphanumeric(text.substr(marker.size() + 1))) {
      return ListMarkerForm::kCombined;
    }
  }
  return ListMarkerForm::kNone;
}

// Returns the standard Unicode bullet code point for a font-specific nested
// list marker, or `std::nullopt` if `code_point` in `font_name` is not one.
// Word's level-2 bullet is a Courier 'o' (◦) and its level-3 bullets are
// Wingdings characters (▪).
std::optional<uint32_t> GetFontBulletCodePoint(uint32_t code_point,
                                               std::string_view font_name) {
  const std::string lower_font_name = base::ToLowerASCII(font_name);
  // Wingdings glyphs are all pictographs and their extracted code points vary
  // by encoding, so any character counts.
  if (lower_font_name.contains(kWingdingsFontSubstring)) {
    return kSquareBulletCodePoint;
  }
  // Courier is a text font, so only 'o' counts as a possible bullet point.
  if (code_point == 'o' && lower_font_name.contains(kCourierFontSubstring)) {
    return 0x25E6;  // ◦
  }
  return std::nullopt;
}

// Returns the parent list level for a marker at `run_x`, or nullptr if there is
// no shallower list level.
//
// Nested-only markers (such as en dashes) must be indented deeper than their
// parent list to be recognized. For the first item in a sublist, the parent is
// the innermost list level. For subsequent sibling items at the same sublist
// level, the innermost list shares the marker's x-coordinate, so scanning
// backwards locates the enclosing parent level.
const ListLevel* GetParentListLevel(base::span<const ListLevel> list_levels,
                                    float run_x) {
  for (const ListLevel& list_level : std::views::reverse(list_levels)) {
    if (run_x > list_level.marker_x + kListIndentThreshold) {
      return &list_level;
    }
  }
  return nullptr;
}

// Returns the font size of the item text for a list marker of `form`. A
// standalone marker's font size can differ from its item text's because some
// authoring tools size bullets separately. For example, PowerPoint sizes
// bullets as a percentage of the text size, so a 20pt item with a 70% bullet
// has a 14pt marker run. Standalone markers therefore use the following run.
float GetListItemFontSize(const TextRunContext& run_context,
                          ListMarkerForm form) {
  if (form == ListMarkerForm::kStandalone) {
    CHECK(run_context.next_run);
    return run_context.next_run->style.font_size;
  }
  return run_context.run->style.font_size;
}

// Classifies `text` as a marker used only for nested list levels, without
// checking its indentation.
ListMarkerForm ClassifyNestedOnlyMarker(std::string_view text,
                                        const TextRunContext& run_context) {
  if (text.empty()) {
    return ListMarkerForm::kNone;
  }

  // LaTeX nested markers (itemize levels 2–4: en dash, asterisk, middle dot).
  const ListMarkerForm nested_marker_form =
      ClassifyUnorderedMarker(text, kNestedOnlyListMarkers, run_context);
  if (nested_marker_form != ListMarkerForm::kNone) {
    return nested_marker_form;
  }

  // Word's remaining nested markers are standalone single non-whitespace
  // characters. Combined "o text" is never accepted because 'o' is also a word.
  size_t i = 0;
  base_icu::UChar32 code_point;
  const bool is_standalone_char =
      base::ReadUnicodeCharacter(text, &i, &code_point) &&
      i + 1 == text.size() && !base::IsUnicodeWhitespace(code_point);
  const bool is_standalone_bullet =
      is_standalone_char &&
      GetFontBulletCodePoint(code_point, run_context.run->style.font_name)
          .has_value();
  if (is_standalone_bullet && IsStandaloneMarkerFollowedByText(run_context)) {
    return ListMarkerForm::kStandalone;
  }
  return ListMarkerForm::kNone;
}

// Returns how a list marker at `x` should be placed relative to `levels`.
ListPlacement GetListPlacement(base::span<const ListLevel> levels,
                               float x,
                               float max_nest) {
  if (levels.empty() || x > levels.back().marker_x + max_nest) {
    return {.kind = ListPlacementKind::kNewList};
  }

  const ListLevel& last = levels.back();
  if (x > last.marker_x + kListIndentThreshold && last.last_item) {
    return {.kind = ListPlacementKind::kNested,
            .level_index = levels.size() - 1};
  }

  size_t target_level = levels.size() - 1;
  while (target_level > 0 &&
         x < levels[target_level].marker_x - kListIndentThreshold) {
    --target_level;
  }

  return {.kind = ListPlacementKind::kExistingLevel,
          .level_index = target_level};
}

// Classifies `run_context` as a decimal ordered list marker: 1-3 digits
// followed by one of `kOrderedMarkerDelimiters` (e.g. "1.", "12)"). On a match,
// returns `ordered_value` set to the number and form `kStandalone` if the
// marker is alone in its run with item text in the next run on the same line,
// or `kCombined` if whitespace and item text follow it in the same run.
// Otherwise, returns form `kNone` with no `ordered_value`, e.g. for years
// ("2023."), decimals ("1.5"), or a marker with no item text after it.
ListMarkerKind ClassifyOrderedMarker(const TextRunContext& run_context) {
  const std::string text = GetTrimmedText(run_context.chars);
  // TODO(crbug.com/525509231): Support alphabetic and roman numeral markers
  // (e.g. "a.", "B)", "iv.").
  size_t i = 0;
  while (i < text.size() && base::IsAsciiDigit(text[i])) {
    ++i;
  }
  if (i == 0 || i > kMaxOrderedMarkerDigits) {
    return {};
  }
  // The marker ends at the first whitespace, or at the end of a standalone run.
  // This also rejects decimals such as "1.5".
  const size_t marker_size =
      std::min(text.find_first_of(base::kWhitespaceASCII), text.size());
  if (!std::ranges::contains(kOrderedMarkerDelimiters,
                             text.substr(i, marker_size - i))) {
    return {};
  }
  int value;
  if (!base::StringToInt(text.substr(0, i), &value)) {
    return {};
  }
  // A run containing only the marker is a standalone marker if the item text
  // follows in the next run on the same line.
  if (marker_size == text.size()) {
    if (!IsStandaloneMarkerFollowedByText(run_context)) {
      return {};
    }
    return {.form = ListMarkerForm::kStandalone, .ordered_value = value};
  }
  // Otherwise, it is a combined run if item text follows the marker.
  if (!ContainsAlphanumeric(text.substr(marker_size))) {
    return {};
  }
  return {.form = ListMarkerForm::kCombined, .ordered_value = value};
}

// Returns whether the first line after the marker run at `marker_run_index`
// that starts with an ordered marker continues
// `marker_kind`'s numbering in the same column, within
// `kMaxOrderedListLookaheadLines` lines.
bool IsOrderedListStartConfirmed(const PageLayoutData& layout,
                                 size_t marker_run_index,
                                 const ListMarkerKind& marker_kind,
                                 float paragraph_spacing_threshold) {
  CHECK(marker_kind.ordered_value.has_value());
  const float marker_x = layout.text_runs[marker_run_index].bounds.x();

  // Start after the item text run, which follows a standalone marker.
  const size_t first_run_index =
      marker_run_index +
      (marker_kind.form == ListMarkerForm::kStandalone ? 2 : 1);
  size_t lines_count = 0;
  for (size_t i = first_run_index; i < layout.text_runs.size(); ++i) {
    const chrome_pdf::AccessibilityTextRunInfo& prev_run =
        layout.text_runs[i - 1];
    const chrome_pdf::AccessibilityTextRunInfo& run = layout.text_runs[i];
    if (AreRunsOnSameLine(prev_run, run)) {
      continue;
    }
    const TextRunContext run_context = GetTextRunContext(layout, i);
    const ListMarkerKind next_marker_kind = ClassifyOrderedMarker(run_context);
    if (next_marker_kind.form != ListMarkerForm::kNone) {
      return std::abs(run.bounds.x() - marker_x) <= kListIndentThreshold &&
             next_marker_kind.ordered_value == *marker_kind.ordered_value + 1;
    }
    // A paragraph gap right after the marker's line suggests a numbered
    // heading followed by body text (e.g. "1. Introduction").
    if (lines_count == 0 && BreakParagraphByLineSpacing(
                                prev_run, run, paragraph_spacing_threshold)) {
      return false;
    }
    if (++lines_count > kMaxOrderedListLookaheadLines) {
      return false;
    }
  }
  return false;
}

// Classifies whether the run at `run_index` is a standalone list marker run or
// combines a list marker with item text.
ListMarkerKind ClassifyListMarker(const PageLayoutData& layout,
                                  size_t run_index,
                                  base::span<const ListLevel> list_levels,
                                  float paragraph_spacing_threshold) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  const TextRunContext run_context = GetTextRunContext(layout, run_index);
  if (!IsLeftToRightRun(*run_context.run)) {
    return {};
  }

  const std::string text = GetTrimmedText(run_context.chars);
  const ListMarkerForm unordered_form =
      ClassifyUnorderedMarker(text, kUnorderedListMarkers, run_context);
  if (unordered_form != ListMarkerForm::kNone) {
    return {.form = unordered_form};
  }

  // Nested-only markers require an enclosing list level and must start right of
  // the parent item's text, so a wrapped line that begins with punctuation such
  // as "–" stays in the item.
  const float run_x = run_context.run->bounds.x();
  const ListLevel* parent = GetParentListLevel(list_levels, run_x);
  if (parent && run_x > parent->item_text_x + kListIndentThreshold) {
    const ListMarkerForm nested_form =
        ClassifyNestedOnlyMarker(text, run_context);
    if (nested_form != ListMarkerForm::kNone) {
      const float max_nest = kMaxListNestingIndentEm *
                             GetListItemFontSize(run_context, nested_form);
      if (run_x <= parent->marker_x + max_nest) {
        return {.form = nested_form};
      }
    }
  }

  const ListMarkerKind ordered_kind = ClassifyOrderedMarker(run_context);
  if (ordered_kind.form == ListMarkerForm::kNone) {
    return {};
  }

  const float item_font_size =
      GetListItemFontSize(run_context, ordered_kind.form);
  const float max_nest = kMaxListNestingIndentEm * item_font_size;
  const ListPlacement placement =
      GetListPlacement(list_levels, run_x, max_nest);

  const int value = *ordered_kind.ordered_value;
  if (placement.kind == ListPlacementKind::kExistingLevel) {
    // Continuing a list requires the next number in sequence.
    if (list_levels[placement.level_index].last_ordered_value != value - 1) {
      return {};
    }
  } else if (value < 1 ||
             !IsOrderedListStartConfirmed(layout, run_index, ordered_kind,
                                          paragraph_spacing_threshold)) {
    // A new list may start at any positive number (e.g. when it resumes on a
    // new page). "0." is excluded because table decimals such as "0.98" are
    // often split into "0." and "98" runs.
    return {};
  }

  return ordered_kind;
}

struct ListMarkerPrefix {
  // Number of leading characters belonging to the marker prefix (leading
  // whitespace, marker chars, and trailing whitespace).
  size_t char_count = 0;
  // Visual width of the marker chars.
  float marker_width = 0.0f;
};

// Returns the prefix info for a combined marker+text run: the number of leading
// chars belonging to the marker (leading whitespace, marker chars, trailing
// whitespace) and the visual width of the marker chars.
ListMarkerPrefix GetListMarkerPrefix(
    base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  ListMarkerPrefix prefix;
  size_t prefix_char_count = 0;
  // Skip leading whitespace before the marker.
  while (prefix_char_count < chars.size() &&
         IsWhitespace(chars[prefix_char_count])) {
    ++prefix_char_count;
  }
  // Record the marker chars' width and skip them. Combined markers are always
  // followed by whitespace (e.g. "• Item", "12. Item").
  while (
      prefix_char_count < chars.size() &&
      !base::IsUnicodeWhitespace(chars[prefix_char_count].unicode_character)) {
    prefix.marker_width +=
        static_cast<float>(chars[prefix_char_count].char_width);
    ++prefix_char_count;
  }
  // Skip trailing whitespace between the marker and the item text.
  while (prefix_char_count < chars.size() &&
         IsWhitespace(chars[prefix_char_count])) {
    ++prefix_char_count;
  }
  prefix.char_count = prefix_char_count;
  return prefix;
}

// Returns the accessible name for the list marker of `kind` that starts
// `run_context`, followed by a space to match Blink's list markers (e.g. "• ",
// "1. "). Ordered markers use the PDF's own number and delimiter. For unordered
// markers, Symbol and Wingdings private-use bullets, Word's Courier 'o', and
// other Wingdings-font markers are mapped to standard Unicode bullets so they
// are spoken and rendered correctly.
std::string GetListMarkerName(const TextRunContext& run_context,
                              const ListMarkerKind& kind) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  if (kind.ordered_value) {
    // A combined run's marker ends at the first whitespace.
    const std::string text = GetTrimmedText(run_context.chars);
    const size_t first_whitespace = text.find_first_of(base::kWhitespaceASCII);
    return base::StrCat(
        {std::string_view(text).substr(0, first_whitespace), " "});
  }
  for (const auto& char_info : run_context.chars) {
    if (IsWhitespace(char_info)) {
      continue;
    }
    uint32_t code_point = char_info.unicode_character;
    if (const auto font_bullet = GetFontBulletCodePoint(
            code_point, run_context.run->style.font_name)) {
      code_point = *font_bullet;
    } else if (const auto it = kPrivateUseBulletMap.find(code_point);
               it != kPrivateUseBulletMap.end()) {
      code_point = it->second;
    }
    std::string name;
    base::WriteUnicodeCharacter(static_cast<base_icu::UChar32>(code_point),
                                &name);
    // Without the space, the marker and item text concatenate into a single
    // token (e.g. "•Item"), preventing word-by-word navigation from separating
    // them and causing speech synthesis to run them together without a pause.
    name += ' ';
    return name;
  }
  // Only called for runs that `ClassifyListMarker()` matched, whose trimmed
  // text starts with a non-whitespace marker, so `chars` always contains a
  // non-whitespace character.
  NOTREACHED();
}

// Returns the total visual width of `chars`.
float GetCharsWidth(base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  float width = 0.0f;
  for (const auto& char_info : chars) {
    width += static_cast<float>(char_info.char_width);
  }
  return width;
}

// Adjusts `text_run` to skip `prefix_char_count` leading characters, shifting
// its visual bounds rightward by `prefix_width`.
void TrimTextRunPrefix(size_t prefix_char_count,
                       float prefix_width,
                       chrome_pdf::AccessibilityTextRunInfo& text_run) {
  CHECK_LE(prefix_char_count, text_run.len);
  text_run.start_index += prefix_char_count;
  text_run.len -= prefix_char_count;
  text_run.bounds.Inset(gfx::InsetsF().set_left(prefix_width));
}

// Returns whether to break before the run at `next_run_index` due to a list
// boundary, such as a new standalone or combined marker run, an
// out-of-sequence ordered marker, or an outdent past the innermost list
// level's indent.
//
// Known limitation: a non-marker block inside a list (e.g. a paragraph under a
// nested item, or text continuing the parent item after a nested list) either
// merges into the previous item's text or ends the list.
// TODO(crbug.com/525509231): Investigate whether it's worth handling this.
bool BreakAtListBoundary(const PageLayoutData& layout,
                         size_t next_run_index,
                         const ui::AXNodeData* block_node,
                         base::span<const ListLevel> list_levels,
                         float paragraph_spacing_threshold) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  CHECK(block_node);
  const ListMarkerKind kind = ClassifyListMarker(
      layout, next_run_index, list_levels, paragraph_spacing_threshold);
  if (kind.form != ListMarkerForm::kNone) {
    return true;
  }
  // The remaining checks only apply when continuing a list item.
  if (block_node->role != ax::mojom::Role::kListItem || list_levels.empty()) {
    return false;
  }
  // Keep out-of-sequence ordered markers from being absorbed as wrapped text.
  const TextRunContext next_run_context =
      GetTextRunContext(layout, next_run_index);
  if (ClassifyOrderedMarker(next_run_context).form != ListMarkerForm::kNone) {
    return true;
  }
  return next_run_context.run->bounds.x() <
         list_levels.back().marker_x - kListIndentThreshold;
}

bool BreakParagraph(uint32_t text_run_index,
                    ui::AXNodeData* block_node,
                    HeadingClassifier heading_classifier,
                    const PageLayoutData& layout,
                    const HeuristicPageProperties& page_properties,
                    base::span<const ListLevel> list_levels) {
  const TextRunContext current_run_context =
      GetTextRunContext(layout, text_run_index);
  const chrome_pdf::AccessibilityTextRunInfo& current_run =
      *current_run_context.run;
  const TextRunContext next_run_context =
      GetTextRunContext(layout, text_run_index + 1);
  const chrome_pdf::AccessibilityTextRunInfo& next_run = *next_run_context.run;

  bool is_large_line_spacing_break = BreakParagraphByLineSpacing(
      current_run, next_run, page_properties.paragraph_spacing_threshold);

  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    if (block_node->role == ax::mojom::Role::kFigure) {
      return false;
    }

    // Header and footer boundaries take precedence over the rules below.
    std::optional<bool> header_footer_break = BreakAtHeaderFooterBoundary(
        current_run_context, next_run_context, page_properties,
        is_large_line_spacing_break, block_node);
    if (header_footer_break.has_value()) {
      return header_footer_break.value();
    }

    if (BreakAtListBoundary(layout, text_run_index + 1, block_node, list_levels,
                            page_properties.paragraph_spacing_threshold)) {
      return true;
    }
  }

  // Use line spacing to determine where to break body text.
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled() ||
      heading_classifier == HeadingClassifier::kNone) {
    return is_large_line_spacing_break;
  }

  // Always break headings at style changes.
  if (!AreStylesAndFontsEquivalent(current_run.style, next_run.style)) {
    return true;
  }

  // For font-size classified headings, break if the next run has a different
  // heading level.
  int current_level =
      block_node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  if (heading_classifier == HeadingClassifier::kFontSize) {
    int next_level = GetHeadingLevelFromSize(
        page_properties.heading_font_size_mapping, next_run.style.font_size);
    return current_level != next_level;
  }

  // For styled headings (e.g. bold, uppercase, font name), break if the next
  // run has a different classifier.
  HeadingClassifier next_classifier =
      GetHeadingClassifier(next_run_context, page_properties);
  return heading_classifier != next_classifier;
}

// Returns whether `text_run` differs in style, language, or position from the
// in-progress static text node, which requires starting a new one so that text
// runs of different styles (e.g. bold vs regular, or superscript vs normal) or
// languages are not merged into a single static text node.
bool BreaksStaticTextNode(const StaticTextState& static_text_state,
                          const chrome_pdf::AccessibilityTextRunInfo& text_run,
                          ax::mojom::TextPosition text_position) {
  // Check for an active node first; when none is open, `language` is empty and
  // comparing it against a non-empty `text_run.language` would falsely report a
  // break on the opening run of a block.
  if (!static_text_state.node) {
    return false;
  }
  // Check `language` before `style` because `language` is tracked on all static
  // text nodes, including inside headings where `style` is left unset.
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
      static_text_state.language != text_run.language) {
    return true;
  }
  // Guard the style and `text_position` checks below, which are only populated
  // for non-heading nodes when the flag is enabled.
  if (!static_text_state.style) {
    return false;
  }
  // `style` is only ever set when the flag is enabled.
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  return !PdfAccessibilityTreeBuilder::AreStylesEquivalent(
             *static_text_state.style, text_run.style) ||
         static_text_state.text_position != text_position;
}

void BuildStaticNode(StaticTextState* static_text_state) {
  // If a static text node is currently being built, finish it before
  // moving on to the next object.
  if (static_text_state->node) {
    static_text_state->node->AddStringAttribute(
        ax::mojom::StringAttribute::kName, static_text_state->text);
    static_text_state->text.clear();
  }
  static_text_state->node = nullptr;
  static_text_state->style.reset();
  static_text_state->text_position.reset();
  static_text_state->language.clear();
}

void ConnectPreviousAndNextOnLine(ui::AXNodeData* previous_on_line_node,
                                  ui::AXNodeData* next_on_line_node) {
  previous_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                         next_on_line_node->id);
  next_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                     previous_on_line_node->id);
}

// Returns whether the candidate block at `candidate_index` was emitted before a
// band block above it in the same column (overlapping it horizontally). Only
// then must the band end above the candidate so sorting can move it. Otherwise
// the candidate joins the band despite a vertical gap, so columns keep stream
// order however the stream orders them (e.g. a table emitted after the column
// beside it).
bool IsEmittedBeforeBlockAboveInColumn(
    base::span<const ui::AXNodeData* const> block_nodes,
    base::span<const size_t> band_indices_above,
    size_t candidate_index) {
  const gfx::RectF& candidate_bounds =
      block_nodes[candidate_index]->relative_bounds.bounds;
  return std::ranges::any_of(band_indices_above, [&](size_t band_index) {
    const gfx::RectF& band_bounds =
        block_nodes[band_index]->relative_bounds.bounds;
    return band_index > candidate_index &&
           band_bounds.x() + kComparisonEpsilon < candidate_bounds.right() &&
           candidate_bounds.x() + kComparisonEpsilon < band_bounds.right();
  });
}

// Reorders `block_nodes` and the matching block entries in `page_child_ids`
// into top-to-bottom reading order. Every block must appear in
// `page_child_ids`, in the same relative order as in `block_nodes`. PDF content
// streams sometimes emit blocks in authoring/layer order (e.g. a footer emitted
// before body text, or a title emitted last). Sorting all blocks purely by `y`
// would scramble multi-column layouts where side-by-side column blocks have
// slightly different top `y` coordinates.
//
// To avoid disrupting columns, blocks are grouped into horizontal bands:
//   1. Blocks whose vertical intervals [y, bottom] overlap share a band.
//   2. A block below a vertical gap also joins the band unless it was emitted
//      before a band block above it in the same column. Gaps between
//      side-by-side columns can line up, and splitting there would interleave
//      the columns by `y`.
//   3. Bands are ordered top-to-bottom across the page.
//   4. Blocks within the same band (such as side-by-side columns) keep their
//      original stream order.
void SortBlockNodesByVerticalBands(std::vector<ui::AXNodeData*>& block_nodes,
                                   std::vector<ui::AXNodeID>& page_child_ids) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
  const size_t num_blocks = block_nodes.size();
  if (num_blocks <= 1) {
    return;
  }

  // Sort a separate `order` vector of `block_nodes` indices (0 .. num_blocks -
  // 1) first: the indices record each block's original stream position (needed
  // to restore stream order within each band after sorting by `y`), while
  // keeping `block_nodes` in its original order to match block slots in
  // `page_child_ids` below.
  //
  // First, sort `order` by each block's top `y` coordinate so vertically
  // overlapping blocks become contiguous (stable, so blocks with equal `y` keep
  // stream order).
  std::vector<size_t> order(num_blocks);
  std::iota(order.begin(), order.end(), 0u);
  std::ranges::stable_sort(order, [&](size_t a, size_t b) {
    return block_nodes[a]->relative_bounds.bounds.y() <
           block_nodes[b]->relative_bounds.bounds.y();
  });

  // Sweep the `y`-sorted blocks top-to-bottom to partition `order` into
  // vertical bands [band_start, i). `band_bottom` tracks the maximum `bottom()`
  // of all blocks in the current band.
  size_t band_start = 0;
  float band_bottom = block_nodes[order[0]]->relative_bounds.bounds.bottom();
  for (size_t i = 1; i < num_blocks; ++i) {
    const gfx::RectF& bounds = block_nodes[order[i]]->relative_bounds.bounds;
    // Whether this block's top edge is above the band's bottom edge, so its
    // vertical interval overlaps the current band.
    const bool overlaps_band = bounds.y() + kComparisonEpsilon < band_bottom;
    if (overlaps_band ||
        !IsEmittedBeforeBlockAboveInColumn(
            block_nodes, base::span(order).subspan(band_start, i - band_start),
            order[i])) {
      // Merge this block into the band.
      band_bottom = std::ranges::max(band_bottom, bounds.bottom());
    } else {
      // End the band [band_start, i) and restore its stream order.
      std::sort(order.begin() + band_start, order.begin() + i);
      band_start = i;
      band_bottom = bounds.bottom();
    }
  }
  // Restore original stream order for the final band.
  std::sort(order.begin() + band_start, order.end());

  // `page_child_ids` defines the child order of the page node in the AXTree and
  // may contain non-block nodes (e.g. OCR wrappers) interleaved with blocks.
  // Replace each block slot in `page_child_ids` with the corresponding block ID
  // from the new band-sorted `order`, leaving non-block IDs in place.
  size_t block_index = 0;
  for (ui::AXNodeID& child_id : page_child_ids) {
    if (block_index < num_blocks && child_id == block_nodes[block_index]->id) {
      child_id = block_nodes[order[block_index]]->id;
      ++block_index;
    }
  }
  CHECK_EQ(block_index, num_blocks);

  std::vector<ui::AXNodeData*> sorted_blocks(num_blocks);
  for (size_t i = 0; i < num_blocks; ++i) {
    sorted_blocks[i] = block_nodes[order[i]];
  }
  block_nodes = std::move(sorted_blocks);
}

}  // namespace

struct PdfAccessibilityTreeBuilderHeuristic::BlockLevelNodeResult {
  raw_ptr<ui::AXNodeData> node = nullptr;
  HeadingClassifier heading_classifier = HeadingClassifier::kNone;
  // Not `kNone` only when `node` is a new `kListItem`.
  ListMarkerKind list_marker_kind;
};

StaticTextState::StaticTextState() = default;
StaticTextState::~StaticTextState() = default;

PdfAccessibilityTreeBuilderHeuristic::PdfAccessibilityTreeBuilderHeuristic(
    PdfAccessibilityTreeBuilder& builder)
    : builder_(builder) {}

PdfAccessibilityTreeBuilderHeuristic::~PdfAccessibilityTreeBuilderHeuristic() =
    default;

void PdfAccessibilityTreeBuilderHeuristic::BuildPageTree() {
  base::ElapsedTimer timer;
  std::optional<float> min_heading_ratio;
  std::optional<float> max_heading_ratio;
  absl::Cleanup run_on_exit = [&timer, &min_heading_ratio, &max_heading_ratio] {
    base::UmaHistogramTimes("Accessibility.PDF.Heuristic.BuildPageTreeTime",
                            timer.Elapsed());
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
        min_heading_ratio && max_heading_ratio) {
      RecordHeadingToBodySizeRatioHistogram(
          "Accessibility.PdfHeuristics.HeadingToBodySizeRatioMax",
          static_cast<int>(std::round(*max_heading_ratio * 100.0f)));
      RecordHeadingToBodySizeRatioHistogram(
          "Accessibility.PdfHeuristics.HeadingToBodySizeRatioMin",
          static_cast<int>(std::round(*min_heading_ratio * 100.0f)));
    }
  };

  const gfx::RectF& page_bounds = builder_->page_node()->relative_bounds.bounds;
  std::vector<chrome_pdf::AccessibilityFormGraphicInfo> graphics;
  std::vector<chrome_pdf::AccessibilityImageInfo> filtered_images;
  base::span<const chrome_pdf::AccessibilityImageInfo> images =
      builder_->images();
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    graphics = FilterGraphics(builder_->graphics(), page_bounds);
    filtered_images = FilterImages(builder_->images());
    images = filtered_images;
  }

  const HeuristicPageProperties page_properties =
      ComputeHeuristicPageProperties(builder_->text_runs(), graphics,
                                     page_bounds);
  const PageLayoutData page_layout = {
      .text_runs = builder_->text_runs(),
      .chars = builder_->chars(),
      .text_run_start_indices = builder_->text_run_start_indices(),
  };

  ui::AXNodeData* block_node = nullptr;
  // Blocks that are direct children of the page node, with each `kList` in
  // place of its `kListItem` blocks.
  std::vector<ui::AXNodeData*> block_nodes;
  ui::AXNodeData* previous_on_line_node = nullptr;
  StaticTextState static_text_state;
  HeadingClassifier current_heading_classifier = HeadingClassifier::kNone;
  // Whether the current block opened with a superscript footnote marker. Set
  // when the block is created, so it is never stale.
  bool block_starts_with_superscript_marker = false;
  uint32_t current_graphic_index = 0;
  LineHelper line_helper(builder_->text_runs());
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool ocr_block = false;
  bool has_ocr_text = false;
#endif

  for (size_t text_run_index = 0; text_run_index < builder_->text_runs().size();
       ++text_run_index) {
    const TextRunContext run_context =
        GetTextRunContext(page_layout, text_run_index);
    const chrome_pdf::AccessibilityTextRunInfo& text_run = *run_context.run;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // OCR text should be marked by nodes before and after it.
    bool ocr_block_start = text_run.is_searchified && !ocr_block;
    bool ocr_block_end = !text_run.is_searchified && ocr_block;
    if (ocr_block_start || ocr_block_end) {
      // If already inside a block, end it.
      // PDF searchifier only processes pages that have no text, hence OCR text
      // is never added in the middle of a paragraph.
      if (block_node) {
        BuildStaticNode(&static_text_state);
        block_node = nullptr;
      }
      CHECK(ocr_block_start || text_run_index);
      gfx::PointF position = ocr_block_start
                                 ? text_run.bounds.origin()
                                 : (builder_->text_runs())[text_run_index - 1]
                                       .bounds.bottom_right();
      builder_->page_node()->child_ids.push_back(
          builder_->CreateOcrWrapperNode(position, ocr_block_start)->id);
      ocr_block = ocr_block_start;
      has_ocr_text = true;
    }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

    // Close any open non-figure block when entering a graphic so the graphic
    // starts its own `kFigure` block even if `BreakParagraph()` didn't split
    // before this run.
    chrome_pdf::AccessibilityFormGraphicInfo* current_graphic = nullptr;
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
        IsObjectWithRangeInTextRun(graphics, current_graphic_index,
                                   text_run_index)) {
      current_graphic = &graphics[current_graphic_index];
      if (block_node && block_node->role != ax::mojom::Role::kFigure) {
        BuildStaticNode(&static_text_state);
        block_node = nullptr;
        current_heading_classifier = HeadingClassifier::kNone;
        previous_on_line_node = nullptr;
      }
    }

    ListMarkerKind list_marker_kind;
    // If we don't have a block level node, create one.
    if (!block_node) {
      BlockLevelNodeResult result = CreateBlockLevelNode(
          page_layout, text_run_index, page_properties, current_graphic);
      block_node = result.node;
      current_heading_classifier = result.heading_classifier;
      list_marker_kind = result.list_marker_kind;
      block_starts_with_superscript_marker =
          GetTextPosition(builder_->text_runs(), text_run_index,
                          run_context.chars) ==
          ax::mojom::TextPosition::kSuperscript;
      // `CreateBlockLevelNode()` already attaches `kListItem` blocks to the
      // enclosing `kList` (which is itself attached to `page_node()`).
      if (list_marker_kind.form == ListMarkerForm::kNone) {
        builder_->page_node()->child_ids.push_back(block_node->id);
        block_nodes.push_back(block_node);
      } else {
        // `CreateBlockLevelNode()` populates `list_levels_` for list items.
        CHECK(!list_levels_.empty());
        if (block_nodes.empty() ||
            block_nodes.back() != list_levels_.front().list) {
          // First item of a new list.
          block_nodes.push_back(list_levels_.front().list);
        }
      }

      if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
          current_heading_classifier != HeadingClassifier::kNone) {
        base::UmaHistogramEnumeration(
            "Accessibility.PdfHeuristics.HeadingClassifier",
            current_heading_classifier);
        if (page_properties.median_font_size > 0) {
          float ratio =
              text_run.style.font_size / page_properties.median_font_size;
          min_heading_ratio =
              std::min(min_heading_ratio.value_or(ratio), ratio);
          max_heading_ratio =
              std::max(max_heading_ratio.value_or(ratio), ratio);
        }
      }
    }

    size_t marker_prefix_char_count = 0;
    if (list_marker_kind.form != ListMarkerForm::kNone) {
      CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
      gfx::RectF marker_bounds = text_run.bounds;
      float item_text_x = 0.0f;
      if (list_marker_kind.form == ListMarkerForm::kCombined) {
        const ListMarkerPrefix prefix = GetListMarkerPrefix(run_context.chars);
        marker_prefix_char_count = prefix.char_count;
        marker_bounds.set_width(prefix.marker_width);
        item_text_x = text_run.bounds.x() +
                      GetCharsWidth(run_context.chars.first(prefix.char_count));
      } else {
        CHECK_EQ(list_marker_kind.form, ListMarkerForm::kStandalone);
        // Standalone markers are only classified when item text follows in
        // `next_run`.
        CHECK(run_context.next_run);
        item_text_x = run_context.next_run->bounds.x();
      }
      list_levels_.back().item_text_x = item_text_x;

      AddListMarkerToNode(GetListMarkerName(run_context, list_marker_kind),
                          marker_bounds, block_node);
      // Standalone runs consist solely of the marker, so proceed to the next
      // run. Combined runs fall through to process the remaining item text.
      if (list_marker_kind.form == ListMarkerForm::kStandalone) {
        continue;
      }
    }

    // If the `text_run_index` is less than or equal to the link's
    // `text_run_index`, then push the link node in the block.
    if (IsObjectWithRangeInTextRun(builder_->links(), current_link_index_,
                                   text_run_index)) {
      BuildStaticNode(&static_text_state);
      const chrome_pdf::AccessibilityLinkInfo& link =
          (builder_->links())[current_link_index_++];
      AddLinkToParaNode(link, block_node, &previous_on_line_node,
                        &text_run_index);

      if (link.text_range.count == 0) {
        continue;
      }

    } else if (IsObjectInTextRun(images, current_image_index_,
                                 text_run_index)) {
      BuildStaticNode(&static_text_state);
      AddImageToParaNode(images[current_image_index_++], block_node,
                         &text_run_index);
      continue;
    } else if (IsObjectWithRangeInTextRun(builder_->highlights(),
                                          current_highlight_index_,
                                          text_run_index)) {
      BuildStaticNode(&static_text_state);
      AddHighlightToParaNode(
          (builder_->highlights())[current_highlight_index_++], block_node,
          &previous_on_line_node, &text_run_index);
    } else {
      // A style, language, or text position change starts a new static text
      // node, which ends any heading the current block was classified as.
      if (BreaksStaticTextNode(
              static_text_state, text_run,
              GetTextPosition(builder_->text_runs(), text_run_index,
                              run_context.chars))) {
        current_heading_classifier = HeadingClassifier::kNone;
      }
      ui::AXNodeData* inline_text_box_node =
          AddTextRunToNode(text_run_index, block_node, &static_text_state,
                           marker_prefix_char_count);

      if (previous_on_line_node) {
        ConnectPreviousAndNextOnLine(previous_on_line_node,
                                     inline_text_box_node);
        // The re-check resolves lines whose opening run was ambiguous. A line
        // opening with a footnote marker is already known to be footnote
        // content, so later runs must not promote it to a header or footer.
        if (!block_starts_with_superscript_marker) {
          UpdateHeaderFooterRoleForSameLineRun(run_context, page_properties,
                                               block_node);
        }
      } else {
        line_helper.StartNewLine(text_run_index);
      }
      line_helper.ProcessNextRun(text_run_index);

      // Accumulate bounds last so the role update above measures the block
      // without this run. Otherwise a right-aligned page number would stretch
      // the block's bounding box across the gap, making a short footer look
      // too wide to promote.
      block_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);
      static_text_state.node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);

      if (text_run_index < builder_->text_runs().size() - 1) {
        if (line_helper.ContinuesLineInReadingOrder(text_run_index + 1)) {
          // The next run continues on the same line in reading order.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line or out of reading order.
          previous_on_line_node = nullptr;
        }
      }
    }

    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      for (const ListLevel& list_level : list_levels_) {
        list_level.list->relative_bounds.bounds.Union(
            block_node->relative_bounds.bounds);
        if (list_level.last_item && list_level.last_item != block_node) {
          list_level.last_item->relative_bounds.bounds.Union(
              block_node->relative_bounds.bounds);
        }
      }
    }

    // A figure block stays open across its entire graphic text range and closes
    // once the last run in the graphic is reached.
    if (block_node->role == ax::mojom::Role::kFigure) {
      const auto& range = graphics[current_graphic_index].text_range;
      if (text_run_index + 1 >= range.index + range.count) {
        BuildStaticNode(&static_text_state);
        block_node = nullptr;
        previous_on_line_node = nullptr;
        ++current_graphic_index;
        continue;
      }
    }

    if (text_run_index == builder_->text_runs().size() - 1) {
      BuildStaticNode(&static_text_state);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(text_run_index, block_node, current_heading_classifier,
                         page_layout, page_properties, list_levels_)) {
        BuildStaticNode(&static_text_state);
        block_node = nullptr;
        current_heading_classifier = HeadingClassifier::kNone;
      }
    }
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Add the wrapper node if still in OCR block and text runs finish.
  if (ocr_block) {
    builder_->page_node()->child_ids.push_back(
        builder_
            ->CreateOcrWrapperNode(
                builder_->text_runs().back().bounds.bottom_right(),
                /*start=*/false)
            ->id);
  }
#endif

  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    // Reorder blocks by vertical bands to restore top-to-bottom reading order
    // for pages emitted in authoring/layer order. Band sorting assumes
    // left-to-right columns, so skip pages with any right-to-left text.
    if (std::ranges::none_of(
            builder_->text_runs(),
            [](const chrome_pdf::AccessibilityTextRunInfo& run) {
              return run.direction ==
                     chrome_pdf::AccessibilityTextDirection::kRightToLeft;
            })) {
      SortBlockNodesByVerticalBands(block_nodes,
                                    builder_->page_node()->child_ids);
      // Remaining annotations go in a new paragraph rather than a trailing
      // figure or list.
      block_node =
          !block_nodes.empty() &&
                  block_nodes.back()->role != ax::mojom::Role::kFigure &&
                  block_nodes.back()->role != ax::mojom::Role::kList
              ? block_nodes.back()
              : nullptr;
    }
    RecordHeaderFooterLocationHistograms(block_nodes, page_properties);
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  AddRemainingAnnotations(images, block_node, has_ocr_text);
#else
  AddRemainingAnnotations(images, block_node);
#endif
}

ui::AXNodeData* PdfAccessibilityTreeBuilderHeuristic::CreateLineBreakingNode(
    ax::mojom::Role role,
    const std::string& language) {
  ui::AXNodeData* node =
      builder_->CreateAndAppendNode(role, ax::mojom::Restriction::kReadOnly);
  node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  current_block_node_ = node;
  current_block_static_text_nodes_.clear();
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
      !language.empty()) {
    node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage, language);
  }
  return node;
}

PdfAccessibilityTreeBuilderHeuristic::BlockLevelNodeResult
PdfAccessibilityTreeBuilderHeuristic::CreateBlockLevelNode(
    const PageLayoutData& layout,
    size_t text_run_index,
    const HeuristicPageProperties& page_properties,
    const chrome_pdf::AccessibilityFormGraphicInfo* graphic) {
  BlockLevelNodeResult result;

  // Reset `list_levels_` so every early return (e.g. figure, side margin,
  // tagged PDF) ends the list unless the list branch below restores it.
  std::vector<ListLevel> levels;
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    std::swap(levels, list_levels_);
  }

  const TextRunContext run_context = GetTextRunContext(layout, text_run_index);
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() && graphic) {
    result.node = CreateLineBreakingNode(ax::mojom::Role::kFigure,
                                         run_context.run->language);
    result.node->relative_bounds.bounds = graphic->bounds;
    return result;
  }

  if (!builder_->mark_headings_using_heuristic()) {
    result.node = CreateLineBreakingNode(ax::mojom::Role::kParagraph,
                                         run_context.run->language);
    return result;
  }

  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
      IsInSideMargin(*run_context.run, page_properties)) {
    result.node = CreateLineBreakingNode(ax::mojom::Role::kSectionFooter,
                                         run_context.run->language);
    return result;
  }

  // Resolving the header and footer role up front also reports whether the run
  // reads as a page number, which decides the ordering below.
  std::optional<ax::mojom::Role> header_footer_ax_role;
  PageNumberKind page_number_kind = PageNumberKind::kNone;
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    header_footer_ax_role = GetAXRoleForHeaderFooterRole(
        GetHeaderFooterRole(run_context, page_properties, &page_number_kind));

    // A standalone or combined marker starts a new `kListItem`. Append it to
    // the list that the previous block belonged to, open a nested list, or
    // start a new list if there is none. The marker run itself becomes the
    // item's `kListMarker` in `BuildPageTree()`.
    if (!header_footer_ax_role.has_value()) {
      result.list_marker_kind =
          ClassifyListMarker(layout, text_run_index, levels,
                             page_properties.paragraph_spacing_threshold);
    }
    if (result.list_marker_kind.form != ListMarkerForm::kNone) {
      result.node = CreateListItemNode(run_context, result.list_marker_kind,
                                       std::move(levels));
      return result;
    }
  }

  ui::AXNodeData* block_node = CreateLineBreakingNode(
      ax::mojom::Role::kParagraph, run_context.run->language);

  // A bare digit in a margin is normally a page number, so skip heading
  // classification to keep a bold or prominent page number from being promoted
  // to a heading.
  bool is_page_number_in_margin =
      page_number_kind == PageNumberKind::kPureNumber &&
      header_footer_ax_role.has_value();

  if (!is_page_number_in_margin) {
    float font_size = run_context.run->style.font_size;
    if (page_properties.heading_font_size_threshold > 0 &&
        font_size > page_properties.heading_font_size_threshold) {
      int heading_level = kDefaultHeadingLevel;
      if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
        int heuristic_heading_level = GetHeadingLevelFromSize(
            page_properties.heading_font_size_mapping, font_size);
        if (IsValidHeadingLevel(heuristic_heading_level)) {
          heading_level = heuristic_heading_level;
        }
      }
      PromoteNodeToHeading(block_node, heading_level);
      result.node = block_node;
      result.heading_classifier = HeadingClassifier::kFontSize;
      return result;
    }

    // Use other styling information to classify headings for text that is
    // smaller than the heading_font_size_threshold.
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      HeadingClassifier classifier =
          GetHeadingClassifier(run_context, page_properties);

      if (classifier != HeadingClassifier::kNone) {
        int heading_level = kLargestStyledHeadingLevel;
        int heuristic_heading_level = GetHeadingLevelFromSize(
            page_properties.heading_font_size_mapping, font_size);
        if (IsValidHeadingLevel(heuristic_heading_level)) {
          heading_level = heuristic_heading_level;
        }
        PromoteNodeToHeading(block_node, heading_level);
        result.node = block_node;
        result.heading_classifier = classifier;
        return result;
      }
    }
  }

  // Reached by page numbers that skipped heading classification and by runs it
  // declined. Real headings in the margins (e.g. section headings at the top of
  // a page or paper titles) have returned above as headings, leaving only
  // non-heading margin text to become a header or footer.
  if (header_footer_ax_role.has_value()) {
    // Only ever set above when the flag is enabled.
    CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
    block_node->role = header_footer_ax_role.value();
  }
  result.node = block_node;
  return result;
}

ui::AXNodeData* PdfAccessibilityTreeBuilderHeuristic::CreateListItemNode(
    const TextRunContext& run_context,
    const ListMarkerKind& marker_kind,
    std::vector<ListLevel> levels) {
  const float marker_x = run_context.run->bounds.x();
  const float max_nest = kMaxListNestingIndentEm *
                         GetListItemFontSize(run_context, marker_kind.form);
  const ListPlacement placement = GetListPlacement(levels, marker_x, max_nest);
  const std::optional<int>& ordered_value = marker_kind.ordered_value;
  const char* list_tag = ordered_value ? "ol" : "ul";

  switch (placement.kind) {
    case ListPlacementKind::kNewList: {
      // Start a new top-level list if no list is currently open, or if the
      // marker is indented far to the right (e.g. crossing into a new column).
      ui::AXNodeData* list =
          CreateLineBreakingNode(ax::mojom::Role::kList, /*language=*/"");
      list->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, list_tag);
      builder_->page_node()->child_ids.push_back(list->id);
      levels = {{.list = list,
                 .marker_x = marker_x,
                 .last_ordered_value = ordered_value}};
      break;
    }
    case ListPlacementKind::kNested: {
      // Deeper indent: start a nested list inside the previous list item,
      // matching Blink's <ul><li>...<ul>...</ul></li></ul> structure.
      levels.resize(placement.level_index + 1);
      CHECK(levels.back().last_item);
      ui::AXNodeData* nested_list =
          CreateLineBreakingNode(ax::mojom::Role::kList, /*language=*/"");
      nested_list->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                      list_tag);
      levels.back().last_item->child_ids.push_back(nested_list->id);
      levels.push_back({.list = nested_list,
                        .marker_x = marker_x,
                        .last_ordered_value = ordered_value});
      break;
    }
    case ListPlacementKind::kExistingLevel:
      // Same indent, or an outdent: pop closed nested levels back to the
      // matching ancestor list. If the marker is still to the left of the
      // outermost level, keep the outermost list to tolerate slight indentation
      // variations, and update its marker position so wrapped lines and
      // sublists are measured against it.
      levels.resize(placement.level_index + 1);
      levels.back().marker_x = std::min(levels.back().marker_x, marker_x);
      levels.back().last_ordered_value = ordered_value;
      break;
  }

  // Append the new item to the active list level and record it for nesting.
  ui::AXNodeData* block_node = CreateLineBreakingNode(
      ax::mojom::Role::kListItem, run_context.run->language);
  block_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "li");
  levels.back().list->child_ids.push_back(block_node->id);
  levels.back().last_item = block_node;
  list_levels_ = std::move(levels);
  return block_node;
}

void PdfAccessibilityTreeBuilderHeuristic::AddListMarkerToNode(
    const std::string& name,
    const gfx::RectF& bounds,
    ui::AXNodeData* item_node) {
  CHECK(!list_levels_.empty());
  ui::AXNodeData* marker = builder_->CreateAndAppendNode(
      ax::mojom::Role::kListMarker, ax::mojom::Restriction::kReadOnly);
  marker->AddStringAttribute(ax::mojom::StringAttribute::kName, name);
  marker->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "::marker");
  // Text run bounds are relative to the page, so offset them by the page node
  // bounds, matching inline text boxes.
  marker->relative_bounds.bounds =
      bounds + builder_->page_node()->relative_bounds.bounds.OffsetFromOrigin();
  item_node->child_ids.push_back(marker->id);
  item_node->relative_bounds.bounds.Union(marker->relative_bounds.bounds);
  list_levels_.back().list->relative_bounds.bounds.Union(
      marker->relative_bounds.bounds);
}

ui::AXNodeData* PdfAccessibilityTreeBuilderHeuristic::AddTextRunToNode(
    size_t text_run_index,
    ui::AXNodeData* parent_node,
    StaticTextState* static_text_state,
    size_t marker_prefix_char_count) {
  chrome_pdf::AccessibilityTextRunInfo text_run =
      builder_->text_runs()[text_run_index];
  chrome_pdf::PageCharacterIndex page_char_index = {
      builder_->page_index(),
      builder_->text_run_start_indices()[text_run_index]};

  if (marker_prefix_char_count > 0) {
    CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());
    const float prefix_width = GetCharsWidth(
        base::span(builder_->chars())
            .subspan(page_char_index.char_index, marker_prefix_char_count));
    TrimTextRunPrefix(marker_prefix_char_count, prefix_width, text_run);
    page_char_index.char_index += marker_prefix_char_count;
  }

  base::span<const chrome_pdf::AccessibilityCharInfo> char_span =
      base::span(builder_->chars())
          .subspan(page_char_index.char_index, text_run.len);
  ax::mojom::TextPosition text_position =
      GetTextPosition(builder_->text_runs(), text_run_index, char_span);

  // If the current block was given the opening run's language and a later run
  // in the block has a different language, move the language attribute from the
  // block node onto its individual static text children.
  ax::mojom::StringAttribute lang_attr = ax::mojom::StringAttribute::kLanguage;
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
      current_block_node_ &&
      current_block_node_->HasStringAttribute(lang_attr)) {
    std::string block_language =
        current_block_node_->GetStringAttribute(lang_attr);
    if (text_run.language != block_language) {
      for (ui::AXNodeData* node : current_block_static_text_nodes_) {
        node->AddStringAttribute(lang_attr, block_language);
      }
      current_block_static_text_nodes_.clear();
      current_block_node_->RemoveStringAttribute(lang_attr);
    }
  }

  if (BreaksStaticTextNode(*static_text_state, text_run, text_position)) {
    BuildStaticNode(static_text_state);
  }

  // This node is for the text inside the parent node, it includes the text of
  // all of the text runs.
  if (!static_text_state->node) {
    // No need to add text styling to the node if it's a heading because the
    // heading has its own styling.
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
        parent_node->role != ax::mojom::Role::kHeading) {
      static_text_state->node = builder_->CreateStaticTextNodeWithStyle(
          page_char_index, text_run.style);
      static_text_state->style = text_run.style;
      static_text_state->text_position = text_position;
      if (text_position != ax::mojom::TextPosition::kNone) {
        static_text_state->node->AddIntAttribute(
            ax::mojom::IntAttribute::kTextPosition,
            static_cast<int32_t>(text_position));
      }
    } else {
      static_text_state->node = builder_->CreateStaticTextNode(page_char_index);
    }
    static_text_state->language = text_run.language;
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      if (current_block_node_ && current_block_node_->HasStringAttribute(
                                     ax::mojom::StringAttribute::kLanguage)) {
        current_block_static_text_nodes_.push_back(static_text_state->node);
      } else if (!text_run.language.empty()) {
        static_text_state->node->AddStringAttribute(
            ax::mojom::StringAttribute::kLanguage, text_run.language);
      }
    }
    parent_node->child_ids.push_back(static_text_state->node->id);
  }

  // Add this text run to the current static text node.
  ui::AXNodeData* inline_text_box_node =
      builder_->CreateInlineTextBoxNode(text_run, page_char_index);
  static_text_state->node->child_ids.push_back(inline_text_box_node->id);

  static_text_state->text += inline_text_box_node->GetStringAttribute(
      ax::mojom::StringAttribute::kName);
  return inline_text_box_node;
}

void PdfAccessibilityTreeBuilderHeuristic::AddTextToAXNode(
    size_t start_text_run_index,
    uint32_t end_text_run_index,
    ui::AXNodeData* ax_node,
    ui::AXNodeData** previous_on_line_node) {
  StaticTextState static_text_state;
  // Accumulate the text of the node.
  std::string ax_name;
  LineHelper line_helper(builder_->text_runs());

  for (size_t text_run_index = start_text_run_index;
       text_run_index <= end_text_run_index; ++text_run_index) {
    ui::AXNodeData* inline_text_box_node =
        AddTextRunToNode(text_run_index, ax_node, &static_text_state);

    static_text_state.node->relative_bounds.bounds.Union(
        inline_text_box_node->relative_bounds.bounds);
    ax_name += inline_text_box_node->GetStringAttribute(
        ax::mojom::StringAttribute::kName);

    if (*previous_on_line_node) {
      ConnectPreviousAndNextOnLine(*previous_on_line_node,
                                   inline_text_box_node);
    } else {
      line_helper.StartNewLine(text_run_index);
    }
    line_helper.ProcessNextRun(text_run_index);

    if (text_run_index < builder_->text_runs().size() - 1) {
      if (line_helper.ContinuesLineInReadingOrder(text_run_index + 1)) {
        // The next run continues on the same line in reading order.
        *previous_on_line_node = inline_text_box_node;
      } else {
        // The next run is on a new line or out of reading order.
        *previous_on_line_node = nullptr;
      }
    }
  }

  BuildStaticNode(&static_text_state);
  ax_node->AddStringAttribute(ax::mojom::StringAttribute::kName, ax_name);
}

void PdfAccessibilityTreeBuilderHeuristic::AddTextToObjectNode(
    size_t object_text_run_index,
    uint32_t object_text_run_count,
    ui::AXNodeData* object_node,
    ui::AXNodeData* para_node,
    ui::AXNodeData** previous_on_line_node,
    size_t* text_run_index) {
  // Annotation objects can overlap in PDF. There can be two overlapping
  // scenarios: Partial overlap and Complete overlap.
  // Partial overlap
  //
  // Link A starts      Link B starts     Link A ends            Link B ends
  //      |a1                |b1               |a2                    |b2
  // -----------------------------------------------------------------------
  //                                    Text
  //
  // Complete overlap
  // Link A starts      Link B starts     Link B ends            Link A ends
  //      |a1                |b1               |b2                    |a2
  // -----------------------------------------------------------------------
  //                                    Text
  //
  // For overlapping annotations, both annotations would store the full
  // text data and nothing will get truncated. For partial overlap, link `A`
  // would contain text between a1 and a2 while link `B` would contain text
  // between b1 and b2. For complete overlap as well, link `A` would contain
  // text between a1 and a2 and link `B` would contain text between b1 and
  // b2. The links would appear in the tree in the order of which they are
  // present. In the tree for both overlapping scenarios, link `A` would
  // appear first in the tree and link `B` after it.

  // If `object_text_run_count` > 0, then the object is part of the page text.
  // Make the text runs contained by the object children of the object node.
  size_t end_text_run_index = object_text_run_index + object_text_run_count;
  uint32_t object_end_text_run_index =
      std::min(end_text_run_index, builder_->text_runs().size()) - 1;
  AddTextToAXNode(object_text_run_index, object_end_text_run_index, object_node,
                  previous_on_line_node);

  para_node->relative_bounds.bounds.Union(object_node->relative_bounds.bounds);

  *text_run_index =
      NormalizeTextRunIndex(object_end_text_run_index, *text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddLinkToParaNode(
    const chrome_pdf::AccessibilityLinkInfo& link,
    ui::AXNodeData* para_node,
    ui::AXNodeData** previous_on_line_node,
    size_t* text_run_index) {
  ui::AXNodeData* link_node = builder_->CreateLinkNode(link);
  para_node->child_ids.push_back(link_node->id);

  // If `link.text_range.count` == 0, then the link is not part of the page
  // text. Push it ahead of the current text run.
  if (link.text_range.count == 0) {
    --(*text_run_index);
    return;
  }

  // Make the text runs contained by the link children of
  // the link node.
  AddTextToObjectNode(link.text_range.index, link.text_range.count, link_node,
                      para_node, previous_on_line_node, text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddImageToParaNode(
    const chrome_pdf::AccessibilityImageInfo& image,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the image's text run
  // index, then push the image ahead of the current text run.
  ui::AXNodeData* image_node = builder_->CreateImageNode(image);
  para_node->child_ids.push_back(image_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddHighlightToParaNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight,
    ui::AXNodeData* para_node,
    ui::AXNodeData** previous_on_line_node,
    size_t* text_run_index) {
  ui::AXNodeData* highlight_node = builder_->CreateHighlightNode(highlight);
  para_node->child_ids.push_back(highlight_node->id);

  // Make the text runs contained by the highlight children of
  // the highlight node.
  AddTextToObjectNode(highlight.text_range.index, highlight.text_range.count,
                      highlight_node, para_node, previous_on_line_node,
                      text_run_index);

  if (!highlight.note_text.empty()) {
    ui::AXNodeData* popup_note_node = builder_->CreatePopupNoteNode(highlight);
    highlight_node->child_ids.push_back(popup_note_node->id);
  }
}

void PdfAccessibilityTreeBuilderHeuristic::AddRemainingAnnotations(
    base::span<const chrome_pdf::AccessibilityImageInfo> images,
    ui::AXNodeData* para_node
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
    bool ocr_applied
#endif
) {
  // If we don't have additional links or images to insert in the tree, then
  // return.
  if (current_link_index_ >= builder_->links().size() &&
      current_image_index_ >= images.size()) {
    return;
  }

  // If we don't have a paragraph node, create a new one.
  if (!para_node) {
    para_node = builder_->CreateAndAppendNode(
        ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
    builder_->page_node()->child_ids.push_back(para_node->id);
  }
  // Push all the links not anchored to any text run to the last paragraph.
  for (size_t i = current_link_index_; i < builder_->links().size(); i++) {
    ui::AXNodeData* link_node =
        builder_->CreateLinkNode((builder_->links())[i]);
    para_node->child_ids.push_back(link_node->id);
  }

  // Push all the images not anchored to any text run to the last paragraph
  // unless OCR has run. PDF Searchify either OCRs all images on a page, or none
  // of them.
  bool push_remaining_images = true;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  push_remaining_images = !ocr_applied;
#endif
  if (push_remaining_images) {
    for (const auto& image : images.subspan(current_image_index_)) {
      ui::AXNodeData* image_node = builder_->CreateImageNode(image);
      para_node->child_ids.push_back(image_node->id);
    }
  }
}

}  // namespace pdf
