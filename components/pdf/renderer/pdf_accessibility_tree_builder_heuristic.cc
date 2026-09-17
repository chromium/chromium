// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_heuristic.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/timer/elapsed_timer.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_features.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

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

// Largest width, as a fraction of the page width, allowed for text in the
// margins to be considered a page number.
constexpr float kMaxPageNumberWidthRatio = 0.30f;

// Font size ratio below which a run counts as substantially smaller than the
// text beside it on the same line, such as a superscript.
constexpr float kSmallTextFontSizeRatio = 0.85f;

// Tolerance used when comparing font sizes, so that sizes that differ only by
// floating point imprecision compare as equal.
constexpr float kFontSizeEpsilon = 0.01f;

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

  // See if it falls within the line (within the threshold).
  float coverage = (clamped_bottom - clamped_top) / height2;
  constexpr float kLineCoverageThreshold = 0.25f;
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

 private:
  void AddRun(const gfx::RectF& run_bounds) {
    float run_width = fabsf(run_bounds.width());
    accumulated_width_ += run_width;
    accumulated_weight_top_ += run_bounds.y() * run_width;
    accumulated_weight_bottom_ +=
        (run_bounds.y() + run_bounds.height()) * run_width;
  }

  void RemoveRun(const gfx::RectF& run_bounds) {
    float run_width = fabsf(run_bounds.width());
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
bool IsObjectInTextRun(const std::vector<T>& objects,
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

bool IsAllUppercase(base::span<const chrome_pdf::AccessibilityCharInfo> chars) {
  bool has_cased_letter = false;
  for (const auto& char_info : chars) {
    UChar32 c = static_cast<UChar32>(char_info.unicode_character);
    if (u_islower(c)) {
      return false;
    }
    if (u_isupper(c)) {
      has_cased_letter = true;
    }
  }
  return has_cased_letter;
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

// The number of characters drawn at a given font size. One entry is recorded
// per text run, so the same font size can appear in more than one entry.
struct FontSizeCharCount {
  float font_size = 0.0f;
  uint32_t char_count = 0;
};

// `font_sizes` holds one entry per text run on the page, sorted ascending by
// font size. Returns the median font size of those entries.
//
// When the heuristic enhancements are enabled, the median is weighted by
// character count so that the text occupying most of the page determines it.
// An unweighted median counts a one character superscript the same as a full
// line of body text, so a page carrying many short runs (page numbers, footnote
// markers, table cells) reports a median below the true body text size. That
// in turn lowers `heading_font_size_threshold` below the body text size and
// promotes ordinary paragraphs to headings.
//
// The returned size is always one of the sizes in `font_sizes` rather than an
// interpolation between two of them, because callers compare font sizes
// against the median for exact equality.
float SelectMedianFontSize(const std::vector<FontSizeCharCount>& font_sizes) {
  CHECK(!font_sizes.empty());
  const float unweighted_median = font_sizes[font_sizes.size() / 2].font_size;
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    return unweighted_median;
  }

  uint64_t total_chars = 0;
  for (const FontSizeCharCount& entry : font_sizes) {
    total_chars += entry.char_count;
  }
  // Without any characters to weight by, every font size carries the same
  // zero weight, and the loop below would return the smallest one. Fall back
  // to the unweighted median instead.
  if (total_chars == 0) {
    return unweighted_median;
  }

  // Return the font size at which the running character count reaches half of
  // the page's characters. The final entry always satisfies this, since by
  // then the running count equals `total_chars`.
  uint64_t accumulated_chars = 0;
  for (const FontSizeCharCount& entry : font_sizes) {
    accumulated_chars += entry.char_count;
    if (accumulated_chars * 2 >= total_chars) {
      return entry.font_size;
    }
  }
  NOTREACHED();
}

void ComputeFontSizes(std::vector<FontSizeCharCount> font_sizes,
                      float* out_heading_font_size_threshold,
                      float* out_median_font_size,
                      std::map<float, int>* out_heading_font_size_mapping) {
  if (font_sizes.size() <= 2) {
    return;
  }

  std::ranges::sort(font_sizes, {}, &FontSizeCharCount::font_size);
  const float median = SelectMedianFontSize(font_sizes);
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
  float current_cluster_font_size = font_sizes.back().font_size;
  bool is_much_larger = current_cluster_font_size >=
                        (*out_median_font_size * kH1MinFontSizeRatio);
  int current_level = is_much_larger ? 1 : 2;
  float min_mapping_font_size = *out_median_font_size;
  // Iterate from the largest font size down to the median font size. The
  // largest font size is compared to itself in the first iteration of the
  // loop so that it's set as the first level.
  for (const FontSizeCharCount& entry : base::Reversed(font_sizes)) {
    const float size = entry.font_size;
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

HeuristicPageProperties ComputeHeuristicPageProperties(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const gfx::RectF& page_bounds) {
  std::vector<FontSizeCharCount> font_sizes;
  std::vector<float> line_spacings;
  std::map<uint32_t, uint32_t> all_color_char_counts;

  for (size_t i = 0; i < text_runs.size(); ++i) {
    const auto& run = text_runs[i];
    font_sizes.push_back(
        {.font_size = run.style.font_size, .char_count = run.len});
    // TODO(crbug.com/525508832): Use a color distance threshold to group
    // visually indistinguishable but non-identical colors together.
    all_color_char_counts[run.style.fill_color] += run.len;

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
  page_properties.max_page_number_width =
      page_bounds.width() * kMaxPageNumberWidthRatio;
  page_properties.top_margin = page_bounds.height() * kHeaderMarginRatio;
  page_properties.bottom_page_number_margin =
      page_bounds.height() * kPageNumberFooterMarginRatio;
  page_properties.bottom_non_page_number_margin =
      page_bounds.height() * kNonPageNumberFooterMarginRatio;
  ComputeFontSizes(std::move(font_sizes),
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

// Returns the header or footer role that `current_run` qualifies for, or
// `kNone` otherwise. A run must sit inside the top or bottom margin band,
// contain at least one alphanumeric character, and be rendered smaller than the
// page's median font size, since running headers and copyright notices are
// often set smaller than body text. Page numbers are exempt from the font size
// rule and are allowed a taller bottom margin band, because they are commonly
// set at body text size and placed higher up the page. `next_run`, the run
// following `current_run` or null at the end of the page, tells whether other
// text shares the same visual line, which separates a page number from a
// footnote marker or a section number.
//
// `out_page_number_kind` receives how the run reads as a page number, so that
// callers can order this against heading classification without classifying the
// run's text a second time.
HeaderFooterRole GetHeaderFooterRole(
    const chrome_pdf::AccessibilityTextRunInfo& current_run,
    const chrome_pdf::AccessibilityTextRunInfo* next_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> current_run_chars,
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
  std::string run_text = GetTrimmedText(current_run_chars);
  if (run_text.empty() || !ContainsAlphanumeric(run_text)) {
    return HeaderFooterRole::kNone;
  }

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

  // A bare digit in a margin is a page number, unless it is much smaller than
  // the text beside it on the same line. That marks a superscript footnote
  // marker, which belongs to the body content, so leave it unclassified.
  if (page_number_kind == PageNumberKind::kPureNumber) {
    if (current_run.style.font_size > 0.0f && has_same_line_neighbor &&
        current_run.style.font_size <
            next_run->style.font_size * kSmallTextFontSizeRatio) {
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
        (page_properties.median_font_size - kFontSizeEpsilon);
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

const chrome_pdf::AccessibilityTextRunInfo* GetRunAfterIndex(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    size_t index) {
  return (index + 1 < text_runs.size()) ? &text_runs[index + 1] : nullptr;
}

bool AreStylesAndFontsEquivalent(
    const chrome_pdf::AccessibilityTextStyleInfo& style1,
    const chrome_pdf::AccessibilityTextStyleInfo& style2) {
  return PdfAccessibilityTreeBuilder::AreStylesEquivalent(style1, style2) &&
         style1.font_name == style2.font_name &&
         style1.fill_color == style2.fill_color;
}

HeadingClassifier GetHeadingClassifier(
    const chrome_pdf::AccessibilityTextRunInfo& current_run,
    const chrome_pdf::AccessibilityTextRunInfo* next_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> current_run_chars,
    const HeuristicPageProperties& page_properties) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());

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
  bool is_run_all_uppercase = IsAllUppercase(current_run_chars);
  if (style.font_size == page_properties.median_font_size) {
    // If this is the last run of the page, label this body text.
    if (!next_run) {
      return HeadingClassifier::kNone;
    }

    // Inline all-caps text (e.g. acronyms on the same line) is body text.
    if (is_run_all_uppercase && is_same_line) {
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
    const chrome_pdf::AccessibilityTextRunInfo& current_run,
    const chrome_pdf::AccessibilityTextRunInfo* next_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> current_run_chars,
    const HeuristicPageProperties& page_properties,
    ui::AXNodeData* block_node) {
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    return;
  }

  // Only the header or footer role matters here. The page number kind is not
  // needed, since heading classification already ran when the block was
  // created.
  PageNumberKind unused_page_number_kind;
  HeaderFooterRole role =
      GetHeaderFooterRole(current_run, next_run, current_run_chars,
                          page_properties, &unused_page_number_kind);

  // Wide text that is not a page number reads as body content, so demote the
  // footer back to a paragraph.
  if (block_node->role == ax::mojom::Role::kSectionFooter &&
      role == HeaderFooterRole::kNone) {
    if (current_run.bounds.width() > page_properties.max_page_number_width) {
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
  float block_page_y =
      block_node->relative_bounds.bounds.y() - page_properties.page_offset_y;
  bool is_footer = role == HeaderFooterRole::kFooter &&
                   block_page_y >= page_properties.bottom_page_number_margin &&
                   block_node->relative_bounds.bounds.width() <=
                       page_properties.max_page_number_width;

  if (is_header || is_footer) {
    // `role` is a header or footer here, so the role always has a value.
    block_node->role = GetAXRoleForHeaderFooterRole(role).value();
  }
}

// Returns whether to break the current block at the header or footer boundary
// `next_run` crosses, or `std::nullopt` when the transition says nothing about
// breaking. Demotes `block_node` back to a paragraph when a block already
// classified as a header or footer turns out to be body content.
std::optional<bool> BreakAtHeaderFooterBoundary(
    const chrome_pdf::AccessibilityTextRunInfo& next_run,
    const chrome_pdf::AccessibilityTextRunInfo* next_next_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> next_run_chars,
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
  PageNumberKind next_page_number_kind = PageNumberKind::kNone;
  HeaderFooterRole next_role =
      GetHeaderFooterRole(next_run, next_next_run, next_run_chars,
                          page_properties, &next_page_number_kind);

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

  float line_spacing = fabsf(next_run.bounds.y() - current_run.bounds.y());
  if (paragraph_spacing_threshold > 0) {
    return line_spacing > paragraph_spacing_threshold;
  }

  // If there's no threshold, that means there weren't enough lines to compute
  // an accurate median, so compare against the line size instead.
  return line_spacing >
         kParagraphLineSpacingRatio * current_run.bounds.height();
}

bool BreakParagraph(uint32_t text_run_index,
                    ui::AXNodeData* block_node,
                    HeadingClassifier heading_classifier,
                    const PageLayoutData& layout,
                    const HeuristicPageProperties& page_properties) {
  const chrome_pdf::AccessibilityTextRunInfo& current_run =
      layout.text_runs[text_run_index];
  const chrome_pdf::AccessibilityTextRunInfo& next_run =
      layout.text_runs[text_run_index + 1];
  const chrome_pdf::AccessibilityTextRunInfo* next_next_run =
      GetRunAfterIndex(layout.text_runs, text_run_index + 1);
  base::span<const chrome_pdf::AccessibilityCharInfo> next_run_chars =
      GetTextRunChars(layout, text_run_index + 1);

  bool is_large_line_spacing_break = BreakParagraphByLineSpacing(
      current_run, next_run, page_properties.paragraph_spacing_threshold);

  // Header and footer boundaries take precedence over the rules below.
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    std::optional<bool> header_footer_break = BreakAtHeaderFooterBoundary(
        next_run, next_next_run, next_run_chars, page_properties,
        is_large_line_spacing_break, block_node);
    if (header_footer_break.has_value()) {
      return header_footer_break.value();
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
  HeadingClassifier next_classifier = GetHeadingClassifier(
      next_run, next_next_run, next_run_chars, page_properties);
  return heading_classifier != next_classifier;
}

void BuildStaticNode(
    ui::AXNodeData** static_text_node,
    std::string* static_text,
    std::optional<chrome_pdf::AccessibilityTextStyleInfo>* current_style) {
  // If a static text node is currently being built, finish it before
  // moving on to the next object.
  if (*static_text_node) {
    (*static_text_node)
        ->AddStringAttribute(ax::mojom::StringAttribute::kName, (*static_text));
    static_text->clear();
  }
  *static_text_node = nullptr;
  current_style->reset();
}

void ConnectPreviousAndNextOnLine(ui::AXNodeData* previous_on_line_node,
                                  ui::AXNodeData* next_on_line_node) {
  previous_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                         next_on_line_node->id);
  next_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                     previous_on_line_node->id);
}

}  // namespace

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

  const HeuristicPageProperties page_properties =
      ComputeHeuristicPageProperties(
          builder_->text_runs(), builder_->page_node()->relative_bounds.bounds);
  const PageLayoutData page_layout = {
      .text_runs = builder_->text_runs(),
      .chars = builder_->chars(),
      .text_run_start_indices = builder_->text_run_start_indices(),
  };

  ui::AXNodeData* block_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  ui::AXNodeData* previous_on_line_node = nullptr;
  std::string static_text;
  std::optional<chrome_pdf::AccessibilityTextStyleInfo> current_style;
  HeadingClassifier current_heading_classifier = HeadingClassifier::kNone;
  LineHelper line_helper(builder_->text_runs());
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool ocr_block = false;
  bool has_ocr_text = false;
#endif

  for (size_t text_run_index = 0; text_run_index < builder_->text_runs().size();
       ++text_run_index) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (builder_->text_runs())[text_run_index];

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // OCR text should be marked by nodes before and after it.
    bool ocr_block_start = text_run.is_searchified && !ocr_block;
    bool ocr_block_end = !text_run.is_searchified && ocr_block;
    if (ocr_block_start || ocr_block_end) {
      // If already inside a block, end it.
      // PDF searchifier only processes pages that have no text, hence OCR text
      // is never added in the middle of a paragraph.
      if (block_node) {
        BuildStaticNode(&static_text_node, &static_text, &current_style);
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
    // If we don't have a block level node, create one.
    if (!block_node) {
      const chrome_pdf::AccessibilityTextRunInfo* next_run =
          GetRunAfterIndex(page_layout.text_runs, text_run_index);
      block_node = CreateBlockLevelNode(
          text_run, next_run, GetTextRunChars(page_layout, text_run_index),
          page_properties, &current_heading_classifier);
      builder_->page_node()->child_ids.push_back(block_node->id);

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

    // If the `text_run_index` is less than or equal to the link's
    // `text_run_index`, then push the link node in the block.
    if (IsObjectWithRangeInTextRun(builder_->links(), current_link_index_,
                                   text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      const chrome_pdf::AccessibilityLinkInfo& link =
          (builder_->links())[current_link_index_++];
      AddLinkToParaNode(link, block_node, &previous_on_line_node,
                        &text_run_index);

      if (link.text_range.count == 0) {
        continue;
      }

    } else if (IsObjectInTextRun(builder_->images(), current_image_index_,
                                 text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      AddImageToParaNode((builder_->images())[current_image_index_++],
                         block_node, &text_run_index);
      continue;
    } else if (IsObjectWithRangeInTextRun(builder_->highlights(),
                                          current_highlight_index_,
                                          text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      AddHighlightToParaNode(
          (builder_->highlights())[current_highlight_index_++], block_node,
          &previous_on_line_node, &text_run_index);
    } else {
      chrome_pdf::PageCharacterIndex page_char_index = {
          builder_->page_index(),
          builder_->text_run_start_indices()[text_run_index]};

      // Under enhanced heuristics, break and start a new static text node if
      // the style changes. This prevents text runs of different styles (e.g.
      // bold vs regular) from being merged into a single static text node.
      if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
          static_text_node && current_style &&
          !PdfAccessibilityTreeBuilder::AreStylesEquivalent(*current_style,
                                                            text_run.style)) {
        BuildStaticNode(&static_text_node, &static_text, &current_style);
        current_heading_classifier = HeadingClassifier::kNone;
      }

      // This node is for the text inside the block, it includes the text of all
      // of the text runs.
      if (!static_text_node) {
        // No need to add text styling to the node if it's a heading because the
        // heading has its own styling.
        if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
            (block_node->role != ax::mojom::Role::kHeading)) {
          static_text_node = builder_->CreateStaticTextNodeWithStyle(
              page_char_index, text_run.style);
          current_style = text_run.style;
        } else {
          static_text_node = builder_->CreateStaticTextNode(page_char_index);
        }
        block_node->child_ids.push_back(static_text_node->id);
      }

      // Add this text run to the current static text node.
      ui::AXNodeData* inline_text_box_node =
          builder_->CreateInlineTextBoxNode(text_run, page_char_index);
      static_text_node->child_ids.push_back(inline_text_box_node->id);

      static_text += inline_text_box_node->GetStringAttribute(
          ax::mojom::StringAttribute::kName);

      if (previous_on_line_node) {
        ConnectPreviousAndNextOnLine(previous_on_line_node,
                                     inline_text_box_node);
        UpdateHeaderFooterRoleForSameLineRun(
            text_run, GetRunAfterIndex(page_layout.text_runs, text_run_index),
            GetTextRunChars(page_layout, text_run_index), page_properties,
            block_node);
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
      static_text_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);

      if (text_run_index < builder_->text_runs().size() - 1) {
        if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
          // The next run is on the same line.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line.
          previous_on_line_node = nullptr;
        }
      }
    }

    if (text_run_index == builder_->text_runs().size() - 1) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(text_run_index, block_node, current_heading_classifier,
                         page_layout, page_properties)) {
        BuildStaticNode(&static_text_node, &static_text, &current_style);
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

  AddRemainingAnnotations(block_node, has_ocr_text);
#else
  AddRemainingAnnotations(block_node);
#endif
}

ui::AXNodeData* PdfAccessibilityTreeBuilderHeuristic::CreateBlockLevelNode(
    const chrome_pdf::AccessibilityTextRunInfo& current_run,
    const chrome_pdf::AccessibilityTextRunInfo* next_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> current_run_chars,
    const HeuristicPageProperties& page_properties,
    HeadingClassifier* out_heading_classifier) {
  ui::AXNodeData* block_node = builder_->CreateAndAppendNode(
      ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
  block_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  *out_heading_classifier = HeadingClassifier::kNone;

  if (!builder_->mark_headings_using_heuristic()) {
    return block_node;
  }

  // Resolving the header and footer role up front also reports whether the run
  // reads as a page number, which decides the ordering below.
  std::optional<ax::mojom::Role> header_footer_ax_role;
  PageNumberKind page_number_kind = PageNumberKind::kNone;
  if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
    header_footer_ax_role = GetAXRoleForHeaderFooterRole(
        GetHeaderFooterRole(current_run, next_run, current_run_chars,
                            page_properties, &page_number_kind));
  }

  // A bare digit in a margin is normally a page number, so skip heading
  // classification to keep a bold or prominent page number from being promoted
  // to a heading.
  bool is_page_number_in_margin =
      page_number_kind == PageNumberKind::kPureNumber &&
      header_footer_ax_role.has_value();

  if (!is_page_number_in_margin) {
    float font_size = current_run.style.font_size;
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
      *out_heading_classifier = HeadingClassifier::kFontSize;
      return block_node;
    }

    // Use other styling information to classify headings for text that is
    // smaller than the heading_font_size_threshold.
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      HeadingClassifier classifier = GetHeadingClassifier(
          current_run, next_run, current_run_chars, page_properties);

      if (classifier != HeadingClassifier::kNone) {
        int heading_level = kLargestStyledHeadingLevel;
        int heuristic_heading_level = GetHeadingLevelFromSize(
            page_properties.heading_font_size_mapping, font_size);
        if (IsValidHeadingLevel(heuristic_heading_level)) {
          heading_level = heuristic_heading_level;
        }
        PromoteNodeToHeading(block_node, heading_level);
        *out_heading_classifier = classifier;
        return block_node;
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
  return block_node;
}

void PdfAccessibilityTreeBuilderHeuristic::AddTextToAXNode(
    size_t start_text_run_index,
    uint32_t end_text_run_index,
    ui::AXNodeData* ax_node,
    ui::AXNodeData** previous_on_line_node) {
  chrome_pdf::PageCharacterIndex page_char_index = {
      builder_->page_index(),
      builder_->text_run_start_indices()[start_text_run_index]};
  ui::AXNodeData* ax_static_text_node =
      builder_->CreateStaticTextNode(page_char_index);
  ax_node->child_ids.push_back(ax_static_text_node->id);
  // Accumulate the text of the node.
  std::string ax_name;
  LineHelper line_helper(builder_->text_runs());

  for (size_t text_run_index = start_text_run_index;
       text_run_index <= end_text_run_index; ++text_run_index) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (builder_->text_runs())[text_run_index];
    page_char_index.char_index =
        builder_->text_run_start_indices()[text_run_index];
    // Add this text run to the current static text node.
    ui::AXNodeData* inline_text_box_node =
        builder_->CreateInlineTextBoxNode(text_run, page_char_index);
    ax_static_text_node->child_ids.push_back(inline_text_box_node->id);

    ax_static_text_node->relative_bounds.bounds.Union(
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
      if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
        // The next run is on the same line.
        *previous_on_line_node = inline_text_box_node;
      } else {
        // The next run is on a new line.
        *previous_on_line_node = nullptr;
      }
    }
  }

  ax_node->AddStringAttribute(ax::mojom::StringAttribute::kName, ax_name);
  ax_static_text_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          ax_name);
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
    ui::AXNodeData* para_node
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
    bool ocr_applied
#endif
) {
  // If we don't have additional links or images to insert in the tree, then
  // return.
  if (current_link_index_ >= builder_->links().size() &&
      current_image_index_ >= builder_->images().size()) {
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
    for (size_t i = current_image_index_; i < builder_->images().size(); i++) {
      const chrome_pdf::AccessibilityImageInfo& image_info =
          (builder_->images())[i];
      ui::AXNodeData* image_node = builder_->CreateImageNode(image_info);
      para_node->child_ids.push_back(image_node->id);
    }
  }
}

}  // namespace pdf
