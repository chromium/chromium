// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_heuristic.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

// Don't try to apply font size thresholds to automatically identify headings
// if the median font size is not at least this many points.
constexpr float kMinimumFontSize = 5.0f;

// Don't try to apply paragraph break thresholds to automatically identify
// paragraph breaks if the median line break is not at least this many points.
constexpr float kMinimumLineSpacing = 5.0f;

// Ratio between the font size of one text run and the median on the page
// for that text run to be considered to be a heading instead of normal text.
constexpr float kHeadingFontSizeRatio = 1.2f;

// Ratio between the line spacing between two lines and the median on the
// page for that line spacing to be considered a paragraph break.
constexpr float kParagraphLineSpacingRatio = 1.2f;

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

// Please keep the below map as close as possible to the list defined in the PDF
// Specification, ISO 32000-1:2008, table 333.
ax::mojom::Role StructureElementTypeToAccessibilityRole(
    const std::string& element_type) {
  static constexpr auto kStructureElementTypeToAccessibilityRoleMap =
      base::MakeFixedFlatMap<std::string_view, ax::mojom::Role>(
          {{"Document", ax::mojom::Role::kDocument},
           {"Part", ax::mojom::Role::kDocPart},
           {"Art", ax::mojom::Role::kArticle},
           {"Sect", ax::mojom::Role::kSection},
           {"Div", ax::mojom::Role::kGenericContainer},
           {"BlockQuote", ax::mojom::Role::kBlockquote},
           {"Caption", ax::mojom::Role::kCaption},
           {"TOC", ax::mojom::Role::kDocToc},
           {"TOCI", ax::mojom::Role::kListItem},
           {"Index", ax::mojom::Role::kDocIndex},
           {"P", ax::mojom::Role::kParagraph},
           {"H", ax::mojom::Role::kHeading},
           {"H1", ax::mojom::Role::kHeading},
           {"H2", ax::mojom::Role::kHeading},
           {"H3", ax::mojom::Role::kHeading},
           {"H4", ax::mojom::Role::kHeading},
           {"H5", ax::mojom::Role::kHeading},
           {"H6", ax::mojom::Role::kHeading},
           {"L", ax::mojom::Role::kList},
           {"LI", ax::mojom::Role::kListItem},
           {"Lbl", ax::mojom::Role::kListMarker},
           {"LBody", ax::mojom::Role::kNone},  // Presentational.
           {"Table", ax::mojom::Role::kTable},
           {"TR", ax::mojom::Role::kRow},
           {"TH", ax::mojom::Role::kRowHeader},
           {"THead", ax::mojom::Role::kRowGroup},
           {"TBody", ax::mojom::Role::kRowGroup},
           {"TFoot", ax::mojom::Role::kRowGroup},
           {"TD", ax::mojom::Role::kCell},
           {"Span", ax::mojom::Role::kStaticText},
           {"Link", ax::mojom::Role::kLink},
           {"Figure", ax::mojom::Role::kFigure},
           {"Formula", ax::mojom::Role::kMath},
           {"Form", ax::mojom::Role::kForm}});

  if (auto iter =
          kStructureElementTypeToAccessibilityRoleMap.find(element_type);
      iter != kStructureElementTypeToAccessibilityRoleMap.end()) {
    return iter->second;
  }
  // Return something that could at least make some sense, other than
  // `kUnknown`.
  return ax::mojom::Role::kParagraph;
}

std::optional<uint32_t> StructureElementTypeToHeadingLevel(
    const std::string& element_type) {
  if (StructureElementTypeToAccessibilityRole(element_type) ==
      ax::mojom::Role::kHeading) {
    if (element_type == "H" || element_type == "H1") {
      return 1;
    } else if (element_type == "H2") {
      return 2;
    } else if (element_type == "H3") {
      return 3;
    } else if (element_type == "H4") {
      return 4;
    } else if (element_type == "H5") {
      return 5;
    } else if (element_type == "H6") {
      return 6;
    }
  }
  return std::nullopt;
}

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

void ComputeParagraphAndHeadingThresholds(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    float* out_heading_font_size_threshold,
    float* out_paragraph_spacing_threshold) {
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
    float median_font_size = font_sizes[font_sizes.size() / 2];
    if (median_font_size > kMinimumFontSize) {
      *out_heading_font_size_threshold =
          median_font_size * kHeadingFontSizeRatio;
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

bool BreakParagraph(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    uint32_t text_run_index,
    float paragraph_spacing_threshold) {
  // Check to see if its also a new paragraph, i.e., if the distance between
  // lines is greater than the threshold.  If there's no threshold, that
  // means there weren't enough lines to compute an accurate median, so
  // we compare against the line size instead.
  float line_spacing = fabsf(text_runs[text_run_index + 1].bounds.y() -
                             text_runs[text_run_index].bounds.y());
  return ((paragraph_spacing_threshold > 0 &&
           line_spacing > paragraph_spacing_threshold) ||
          (paragraph_spacing_threshold == 0 &&
           line_spacing > kParagraphLineSpacingRatio *
                              text_runs[text_run_index].bounds.height()));
}

void BuildStaticNode(ui::AXNodeData** static_text_node,
                     std::string* static_text) {
  // If we're in the middle of building a static text node, finish it before
  // moving on to the next object.
  if (*static_text_node) {
    (*static_text_node)
        ->AddStringAttribute(ax::mojom::StringAttribute::kName, (*static_text));
    static_text->clear();
  }
  *static_text_node = nullptr;
}

void ConnectPreviousAndNextOnLine(ui::AXNodeData* previous_on_line_node,
                                  ui::AXNodeData* next_on_line_node) {
  previous_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                         next_on_line_node->id);
  next_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                     previous_on_line_node->id);
}

}  // namespace

namespace pdf {

PdfAccessibilityTreeBuilderHeuristic::PdfAccessibilityTreeBuilderHeuristic(
    PdfAccessibilityTreeBuilder& builder)
    : builder_(builder) {}

void PdfAccessibilityTreeBuilderHeuristic::BuildPageTree() {
  ComputeParagraphAndHeadingThresholds(*builder_->text_runs_,
                                       &heading_font_size_threshold_,
                                       &paragraph_spacing_threshold_);

  ui::AXNodeData* block_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  ui::AXNodeData* previous_on_line_node = nullptr;
  std::string static_text;
  LineHelper line_helper(*builder_->text_runs_);
  bool pdf_forms_enabled =
      base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool ocr_block = false;
  bool has_ocr_text = false;
#endif

  for (size_t text_run_index = 0; text_run_index < builder_->text_runs_->size();
       ++text_run_index) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (*builder_->text_runs_)[text_run_index];

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // OCR text should be marked by nodes before and after it.
    bool ocr_block_start = text_run.is_searchified && !ocr_block;
    bool ocr_block_end = !text_run.is_searchified && ocr_block;
    if (ocr_block_start || ocr_block_end) {
      // If already inside a block, end it.
      // PDF searchifier only processes pages that have no text, hence OCR text
      // is never added in the middle of a paragraph.
      if (block_node) {
        BuildStaticNode(&static_text_node, &static_text);
        block_node = nullptr;
      }
      CHECK(ocr_block_start || text_run_index);
      gfx::PointF position = ocr_block_start
                                 ? text_run.bounds.origin()
                                 : (*builder_->text_runs_)[text_run_index - 1]
                                       .bounds.bottom_right();
      builder_->page_node_->child_ids.push_back(
          builder_->CreateOcrWrapperNode(position, ocr_block_start)->id);
      ocr_block = ocr_block_start;
      has_ocr_text = true;
    }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // If we don't have a block level node, create one.
    if (!block_node) {
      block_node =
          CreateBlockLevelNode(text_run.tag_type, text_run.style.font_size);
      builder_->page_node_->child_ids.push_back(block_node->id);
    }

    // If the `text_run_index` is less than or equal to the link's
    // `text_run_index`, then push the link node in the block.
    if (IsObjectWithRangeInTextRun(*builder_->links_, current_link_index_,
                                   text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      const chrome_pdf::AccessibilityLinkInfo& link =
          (*builder_->links_)[current_link_index_++];
      AddLinkToParaNode(link, block_node, &previous_on_line_node,
                        &text_run_index);

      if (link.text_range.count == 0) {
        continue;
      }

    } else if (IsObjectInTextRun(*builder_->images_, current_image_index_,
                                 text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      AddImageToParaNode((*builder_->images_)[current_image_index_++],
                         block_node, &text_run_index);
      continue;
    } else if (IsObjectWithRangeInTextRun(*builder_->highlights_,
                                          current_highlight_index_,
                                          text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      AddHighlightToParaNode(
          (*builder_->highlights_)[current_highlight_index_++], block_node,
          &previous_on_line_node, &text_run_index);
    } else if (IsObjectInTextRun(*builder_->text_fields_,
                                 current_text_field_index_, text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddTextFieldToParaNode(
          (*builder_->text_fields_)[current_text_field_index_++], block_node,
          &text_run_index);
      continue;
    } else if (IsObjectInTextRun(*builder_->buttons_, current_button_index_,
                                 text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddButtonToParaNode((*builder_->buttons_)[current_button_index_++],
                          block_node, &text_run_index);
      continue;
    } else if (IsObjectInTextRun(*builder_->choice_fields_,
                                 current_choice_field_index_, text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddChoiceFieldToParaNode(
          (*builder_->choice_fields_)[current_choice_field_index_++],
          block_node, &text_run_index);
      continue;
    } else {
      chrome_pdf::PageCharacterIndex page_char_index = {
          builder_->page_index_,
          builder_->text_run_start_indices_[text_run_index]};

      // This node is for the text inside the block, it includes the text of all
      // of the text runs.
      if (!static_text_node) {
        static_text_node = builder_->CreateStaticTextNode(page_char_index);
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

      if (text_run_index < builder_->text_runs_->size() - 1) {
        if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
          // The next run is on the same line.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line.
          previous_on_line_node = nullptr;
        }
      }
    }

    if (text_run_index == builder_->text_runs_->size() - 1) {
      BuildStaticNode(&static_text_node, &static_text);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(*builder_->text_runs_, text_run_index,
                         paragraph_spacing_threshold_)) {
        BuildStaticNode(&static_text_node, &static_text);
        block_node = nullptr;
      }
    }
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Add the wrapper node if still in OCR block and text runs finish.
  if (ocr_block) {
    builder_->page_node_->child_ids.push_back(
        builder_
            ->CreateOcrWrapperNode(
                builder_->text_runs_->back().bounds.bottom_right(),
                /*start=*/false)
            ->id);
  }

  AddRemainingAnnotations(block_node, has_ocr_text);
#else
  AddRemainingAnnotations(block_node);
#endif
}

ui::AXNodeData* PdfAccessibilityTreeBuilderHeuristic::CreateBlockLevelNode(
    const std::string& text_run_type,
    float font_size) {
  ui::AXNodeData* block_node = builder_->CreateAndAppendNode(
      StructureElementTypeToAccessibilityRole(text_run_type),
      ax::mojom::Restriction::kReadOnly);
  block_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  if (std::optional<uint32_t> level =
          StructureElementTypeToHeadingLevel(text_run_type);
      level) {
    block_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                *level);
    // TODO(crbug.com/40707542): Set the HTML tag to "h*" by creating a helper
    // in `AXEnumUtils`.
  }

  if (builder_->mark_headings_using_heuristic_ &&
      heading_font_size_threshold_ > 0 &&
      font_size > heading_font_size_threshold_) {
    block_node->role = ax::mojom::Role::kHeading;
    block_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 2);
    block_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h2");
  }

  return block_node;
}

void PdfAccessibilityTreeBuilderHeuristic::AddTextToAXNode(
    size_t start_text_run_index,
    uint32_t end_text_run_index,
    ui::AXNodeData* ax_node,
    ui::AXNodeData** previous_on_line_node) {
  chrome_pdf::PageCharacterIndex page_char_index = {
      builder_->page_index_,
      builder_->text_run_start_indices_[start_text_run_index]};
  ui::AXNodeData* ax_static_text_node =
      builder_->CreateStaticTextNode(page_char_index);
  ax_node->child_ids.push_back(ax_static_text_node->id);
  // Accumulate the text of the node.
  std::string ax_name;
  LineHelper line_helper(*builder_->text_runs_);

  for (size_t text_run_index = start_text_run_index;
       text_run_index <= end_text_run_index; ++text_run_index) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (*builder_->text_runs_)[text_run_index];
    page_char_index.char_index =
        builder_->text_run_start_indices_[text_run_index];
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

    if (text_run_index < builder_->text_runs_->size() - 1) {
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
      std::min(end_text_run_index, builder_->text_runs_->size()) - 1;
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
  if (current_link_index_ >= builder_->links_->size() &&
      current_image_index_ >= builder_->images_->size() &&
      current_text_field_index_ >= builder_->text_fields_->size() &&
      current_button_index_ >= builder_->buttons_->size() &&
      current_choice_field_index_ >= builder_->choice_fields_->size()) {
    return;
  }

  // If we don't have a paragraph node, create a new one.
  if (!para_node) {
    para_node = builder_->CreateAndAppendNode(
        ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
    builder_->page_node_->child_ids.push_back(para_node->id);
  }
  // Push all the links not anchored to any text run to the last paragraph.
  for (size_t i = current_link_index_; i < builder_->links_->size(); i++) {
    ui::AXNodeData* link_node =
        builder_->CreateLinkNode((*builder_->links_)[i]);
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
    for (size_t i = current_image_index_; i < builder_->images_->size(); i++) {
      const chrome_pdf::AccessibilityImageInfo& image_info =
          (*builder_->images_)[i];
      ui::AXNodeData* image_node = builder_->CreateImageNode(image_info);
      para_node->child_ids.push_back(image_node->id);
    }
  }

  if (base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm)) {
    // Push all the text fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_text_field_index_;
         i < builder_->text_fields_->size(); i++) {
      ui::AXNodeData* text_field_node =
          builder_->CreateTextFieldNode((*builder_->text_fields_)[i]);
      para_node->child_ids.push_back(text_field_node->id);
    }

    // Push all the buttons not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_button_index_; i < builder_->buttons_->size();
         i++) {
      ui::AXNodeData* button_node =
          builder_->CreateButtonNode((*builder_->buttons_)[i]);
      para_node->child_ids.push_back(button_node->id);
    }

    // Push all the choice fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_choice_field_index_;
         i < builder_->choice_fields_->size(); i++) {
      ui::AXNodeData* choice_field_node =
          builder_->CreateChoiceFieldNode((*builder_->choice_fields_)[i]);
      para_node->child_ids.push_back(choice_field_node->id);
    }
  }
}

}  // namespace pdf
