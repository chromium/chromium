// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_heuristic.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
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

// The smallest heading level allowed (corresponds to <h6>).
constexpr int kSmallestHeadingLevel = 6;

// Font weight for semi-bold text. Used to determine if the run could be a
// heading.
constexpr int kSemiBoldWeight = 600;

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

    // Look at the next run, and determine how much it overlaps the line.
    const auto& run_bounds = (*text_runs_)[run_index].bounds;
    if (run_bounds.height() == 0.0f) {
      return false;
    }

    float clamped_top = std::max(line_top, run_bounds.y());
    float clamped_bottom =
        std::min(line_bottom, run_bounds.y() + run_bounds.height());
    if (clamped_bottom < clamped_top) {
      return false;
    }

    float coverage = (clamped_bottom - clamped_top) / (run_bounds.height());

    // See if it falls within the line (within our threshold).
    constexpr float kLineCoverageThreshold = 0.25f;
    return coverage > kLineCoverageThreshold;
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

void ComputeParagraphAndHeadingThresholds(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    float* out_heading_font_size_threshold,
    float* out_paragraph_spacing_threshold,
    float* out_median_font_size,
    std::map<float, int>* out_heading_font_size_mapping) {
  // Scan over the font sizes and line spacing within this page and
  // set heuristic thresholds so that text larger than the median font
  // size can be marked as a heading, and spacing larger than the median
  // line spacing can be a paragraph break.
  std::vector<float> font_sizes;
  std::vector<float> line_spacings;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    font_sizes.push_back(text_runs[i].style.font_size);
    if (i > 0) {
      const auto& cur = text_runs[i].bounds;
      const auto& prev = text_runs[i - 1].bounds;
      if (cur.y() > prev.y() + prev.height() / 2) {
        line_spacings.push_back(cur.y() - prev.y());
      }
    }
  }
  if (font_sizes.size() > 2) {
    std::sort(font_sizes.begin(), font_sizes.end());
    *out_median_font_size = font_sizes[font_sizes.size() / 2];
    if (*out_median_font_size > kMinimumFontSize) {
      *out_heading_font_size_threshold =
          *out_median_font_size * kHeadingFontSizeRatio;

      if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
        CHECK(out_heading_font_size_mapping->empty());
        // Start at heading level 1 only if the font size is significantly
        // larger than the median.
        float current_cluster_font_size = font_sizes.back();
        bool is_much_larger = current_cluster_font_size >=
                              (*out_median_font_size * kH1MinFontSizeRatio);
        int current_level = is_much_larger ? 1 : 2;
        float min_mapping_font_size = *out_median_font_size;
        // Iterate from the largest font size down to the median font size. The
        // largest font size is compared to itself in the first iteration of the
        // loop so that it's set as the first level.
        for (float size : base::Reversed(font_sizes)) {
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
    }
  }
  if (line_spacings.size() > 4) {
    std::sort(line_spacings.begin(), line_spacings.end());
    float median_line_spacing = line_spacings[line_spacings.size() / 2];
    if (median_line_spacing > kMinimumLineSpacing) {
      *out_paragraph_spacing_threshold =
          median_line_spacing * kParagraphLineSpacingRatio;
    }
  }
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

// Returns a span of AccessibilityCharInfo corresponding to the text run at
// `text_run_index`.
base::span<const chrome_pdf::AccessibilityCharInfo> GetTextRunChars(
    const PageLayoutData& layout,
    size_t text_run_index) {
  uint32_t start_index = layout.text_run_start_indices[text_run_index];
  uint32_t len = layout.text_runs[text_run_index].len;
  return base::span(layout.chars).subspan(start_index, len);
}

bool AreStylesAndFontsEquivalent(
    const chrome_pdf::AccessibilityTextStyleInfo& style1,
    const chrome_pdf::AccessibilityTextStyleInfo& style2) {
  return PdfAccessibilityTreeBuilder::AreStylesEquivalent(style1, style2) &&
         style1.font_name == style2.font_name;
}

HeadingClassifier GetHeadingClassifier(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    base::span<const chrome_pdf::AccessibilityCharInfo> text_run_chars,
    float median_font_size) {
  CHECK(features::IsPdfAccessibilityHeuristicEnhancementsEnabled());

  const chrome_pdf::AccessibilityTextStyleInfo& style = text_run.style;
  if (style.font_size < median_font_size) {
    return HeadingClassifier::kNone;
  }

  // Check all-caps before bold styling because if a run has both, the all caps
  // classification should take precedence.
  if (IsAllUppercase(text_run_chars)) {
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

  return HeadingClassifier::kNone;
}

void PromoteNodeToHeading(ui::AXNodeData* block_node, int heading_level) {
  block_node->role = ax::mojom::Role::kHeading;
  block_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                              heading_level);
  block_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                 "h" + base::NumberToString(heading_level));
}

bool BreakParagraph(uint32_t text_run_index,
                    const ui::AXNodeData* block_node,
                    HeadingClassifier heading_classifier,
                    const PageLayoutData& layout,
                    const HeuristicThresholds& thresholds) {
  const chrome_pdf::AccessibilityTextRunInfo& current_run =
      layout.text_runs[text_run_index];
  const chrome_pdf::AccessibilityTextRunInfo& next_run =
      layout.text_runs[text_run_index + 1];

  // Use line spacing to determine where to break body text.
  if (!features::IsPdfAccessibilityHeuristicEnhancementsEnabled() ||
      heading_classifier == HeadingClassifier::kNone) {
    float line_spacing = fabsf(next_run.bounds.y() - current_run.bounds.y());
    if (thresholds.paragraph_spacing_threshold > 0) {
      return line_spacing > thresholds.paragraph_spacing_threshold;
    }

    // If there's no threshold, that means there weren't enough lines to compute
    // an accurate median, so compare against the line size instead.
    return line_spacing >
           kParagraphLineSpacingRatio * current_run.bounds.height();
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
        *thresholds.heading_font_size_mapping, next_run.style.font_size);
    return current_level != next_level;
  }

  HeadingClassifier next_classifier = GetHeadingClassifier(
      layout.text_runs[text_run_index + 1],
      GetTextRunChars(layout, text_run_index + 1), thresholds.median_font_size);
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
  absl::Cleanup run_on_exit = [&timer] {
    base::UmaHistogramTimes("Accessibility.PDF.Heuristic.BuildPageTreeTime",
                            timer.Elapsed());
  };
  float heading_font_size_threshold = 0;
  float paragraph_spacing_threshold = 0;
  float median_font_size = 0;
  std::map<float, int> font_size_heading_mapping;
  ComputeParagraphAndHeadingThresholds(
      builder_->text_runs(), &heading_font_size_threshold,
      &paragraph_spacing_threshold, &median_font_size,
      &font_size_heading_mapping);

  const PageLayoutData layout = {
      .text_runs = builder_->text_runs(),
      .chars = builder_->chars(),
      .text_run_start_indices = builder_->text_run_start_indices(),
  };

  const HeuristicThresholds thresholds = {
      .paragraph_spacing_threshold = paragraph_spacing_threshold,
      .heading_font_size_mapping = raw_ref(font_size_heading_mapping),
      .median_font_size = median_font_size,
      .heading_font_size_threshold = heading_font_size_threshold,
  };

  ui::AXNodeData* block_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  ui::AXNodeData* previous_on_line_node = nullptr;
  std::string static_text;
  std::optional<chrome_pdf::AccessibilityTextStyleInfo> current_style;
  HeadingClassifier current_heading_classifier = HeadingClassifier::kNone;
  LineHelper line_helper(builder_->text_runs());
  bool pdf_forms_enabled =
      base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm);
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
      block_node = CreateBlockLevelNode(text_run.style.font_size, thresholds);
      builder_->page_node()->child_ids.push_back(block_node->id);
      if (block_node->role == ax::mojom::Role::kHeading) {
        current_heading_classifier = HeadingClassifier::kFontSize;
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
    } else if (IsObjectInTextRun(builder_->text_fields(),
                                 current_text_field_index_, text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      AddTextFieldToParaNode(
          (builder_->text_fields())[current_text_field_index_++], block_node,
          &text_run_index);
      continue;
    } else if (IsObjectInTextRun(builder_->buttons(), current_button_index_,
                                 text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      AddButtonToParaNode((builder_->buttons())[current_button_index_++],
                          block_node, &text_run_index);
      continue;
    } else if (IsObjectInTextRun(builder_->choice_fields(),
                                 current_choice_field_index_, text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      AddChoiceFieldToParaNode(
          (builder_->choice_fields())[current_choice_field_index_++],
          block_node, &text_run_index);
      continue;
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

      block_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);
      static_text_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);

      if (previous_on_line_node) {
        ConnectPreviousAndNextOnLine(previous_on_line_node,
                                     inline_text_box_node);
      } else {
        line_helper.StartNewLine(text_run_index);
      }
      line_helper.ProcessNextRun(text_run_index);

      if (text_run_index < builder_->text_runs().size() - 1) {
        if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
          // The next run is on the same line.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line.
          previous_on_line_node = nullptr;

          // If this run satisfies certain styling requirements and is on its
          // own line, make it a heading.
          if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled() &&
              current_heading_classifier == HeadingClassifier::kNone) {
            current_heading_classifier = GetHeadingClassifier(
                text_run, GetTextRunChars(layout, text_run_index),
                thresholds.median_font_size);
            if (current_heading_classifier != HeadingClassifier::kNone) {
              int heading_level =
                  GetHeadingLevelFromSize(*thresholds.heading_font_size_mapping,
                                          text_run.style.font_size);
              PromoteNodeToHeading(block_node, heading_level);
            }
          }
        }
      }
    }

    if (text_run_index == builder_->text_runs().size() - 1) {
      BuildStaticNode(&static_text_node, &static_text, &current_style);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(text_run_index, block_node, current_heading_classifier,
                         layout, thresholds)) {
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
    float font_size,
    const HeuristicThresholds& thresholds) {
  ui::AXNodeData* block_node = builder_->CreateAndAppendNode(
      ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
  block_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);

  if (builder_->mark_headings_using_heuristic() &&
      thresholds.heading_font_size_threshold > 0 &&
      font_size > thresholds.heading_font_size_threshold) {
    int heading_level = kDefaultHeadingLevel;
    if (features::IsPdfAccessibilityHeuristicEnhancementsEnabled()) {
      int heuristic_heading_level = GetHeadingLevelFromSize(
          *thresholds.heading_font_size_mapping, font_size);
      if (heuristic_heading_level >= 1 && heuristic_heading_level <= 6) {
        heading_level = heuristic_heading_level;
      }
    }
    PromoteNodeToHeading(block_node, heading_level);
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

void PdfAccessibilityTreeBuilderHeuristic::AddTextFieldToParaNode(
    const chrome_pdf::AccessibilityTextFieldInfo& text_field,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the text_field's text
  // run index, then push the text_field ahead of the current text run.
  ui::AXNodeData* text_field_node = builder_->CreateTextFieldNode(text_field);
  para_node->child_ids.push_back(text_field_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddButtonToParaNode(
    const chrome_pdf::AccessibilityButtonInfo& button,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the button's text
  // run index, then push the button ahead of the current text run.
  ui::AXNodeData* button_node = builder_->CreateButtonNode(button);
  para_node->child_ids.push_back(button_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddChoiceFieldToParaNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the choice_field's text
  // run index, then push the choice_field ahead of the current text run.
  ui::AXNodeData* choice_field_node =
      builder_->CreateChoiceFieldNode(choice_field);
  para_node->child_ids.push_back(choice_field_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilderHeuristic::AddRemainingAnnotations(
    ui::AXNodeData* para_node
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
    bool ocr_applied
#endif
) {
  // If we don't have additional links, images or form fields to insert in the
  // tree, then return.
  if (current_link_index_ >= builder_->links().size() &&
      current_image_index_ >= builder_->images().size() &&
      current_text_field_index_ >= builder_->text_fields().size() &&
      current_button_index_ >= builder_->buttons().size() &&
      current_choice_field_index_ >= builder_->choice_fields().size()) {
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

  if (base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm)) {
    // Push all the text fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_text_field_index_;
         i < builder_->text_fields().size(); i++) {
      ui::AXNodeData* text_field_node =
          builder_->CreateTextFieldNode((builder_->text_fields())[i]);
      para_node->child_ids.push_back(text_field_node->id);
    }

    // Push all the buttons not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_button_index_; i < builder_->buttons().size();
         i++) {
      ui::AXNodeData* button_node =
          builder_->CreateButtonNode((builder_->buttons())[i]);
      para_node->child_ids.push_back(button_node->id);
    }

    // Push all the choice fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_choice_field_index_;
         i < builder_->choice_fields().size(); i++) {
      ui::AXNodeData* choice_field_node =
          builder_->CreateChoiceFieldNode((builder_->choice_fields())[i]);
      para_node->child_ids.push_back(choice_field_node->id);
    }
  }
}

}  // namespace pdf
