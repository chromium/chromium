// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"

#include <queue>
#include <string>

#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/pdf/renderer/pdf_ocr_helper.h"
#include "components/strings/grit/components_strings.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

namespace {

// Don't try to apply font size thresholds to automatically identify headings
// if the median font size is not at least this many points.
const float kMinimumFontSize = 5.0f;

// Don't try to apply paragraph break thresholds to automatically identify
// paragraph breaks if the median line break is not at least this many points.
const float kMinimumLineSpacing = 5.0f;

// Ratio between the font size of one text run and the median on the page
// for that text run to be considered to be a heading instead of normal text.
const float kHeadingFontSizeRatio = 1.2f;

// Ratio between the line spacing between two lines and the median on the
// page for that line spacing to be considered a paragraph break.
const float kParagraphLineSpacingRatio = 1.2f;

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

void ConnectPreviousAndNextOnLine(ui::AXNodeData* previous_on_line_node,
                                  ui::AXNodeData* next_on_line_node) {
  previous_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                         next_on_line_node->id);
  next_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                     previous_on_line_node->id);
}

ax::mojom::Role GetRoleForButtonType(chrome_pdf::ButtonType button_type) {
  switch (button_type) {
    case chrome_pdf::ButtonType::kRadioButton:
      return ax::mojom::Role::kRadioButton;
    case chrome_pdf::ButtonType::kCheckBox:
      return ax::mojom::Role::kCheckBox;
    case chrome_pdf::ButtonType::kPushButton:
      return ax::mojom::Role::kButton;
  }
}

std::string GetTextRunCharsAsUTF8(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    int char_index) {
  std::string chars_utf8;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    base::WriteUnicodeCharacter(
        static_cast<base_icu::UChar32>(chars[char_index + i].unicode_character),
        &chars_utf8);
  }
  return chars_utf8;
}

std::vector<int32_t> GetTextRunCharOffsets(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    int char_index) {
  std::vector<int32_t> char_offsets(text_run.len);
  double offset = 0.0;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    offset += chars[char_index + i].char_width;
    char_offsets[i] = floor(offset);
  }
  return char_offsets;
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

bool IsTextRenderModeFill(const chrome_pdf::AccessibilityTextRenderMode& mode) {
  switch (mode) {
    case chrome_pdf::AccessibilityTextRenderMode::kFill:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kFillClip:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStrokeClip:
      return true;
    default:
      return false;
  }
}

bool IsTextRenderModeStroke(
    const chrome_pdf::AccessibilityTextRenderMode& mode) {
  switch (mode) {
    case chrome_pdf::AccessibilityTextRenderMode::kStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStroke:
    case chrome_pdf::AccessibilityTextRenderMode::kStrokeClip:
    case chrome_pdf::AccessibilityTextRenderMode::kFillStrokeClip:
      return true;
    default:
      return false;
  }
}

size_t NormalizeTextRunIndex(uint32_t object_end_text_run_index,
                             size_t current_text_run_index) {
  return std::max<size_t>(
      object_end_text_run_index,
      current_text_run_index ? current_text_run_index - 1 : 0);
}

}  // namespace

namespace pdf {

PdfAccessibilityTreeBuilder::PdfAccessibilityTreeBuilder(
    base::WeakPtr<PdfAccessibilityTree> pdf_accessibility_tree,
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects,
    const chrome_pdf::AccessibilityPageInfo& page_info,
    uint32_t page_index,
    ui::AXNodeData* root_node,
    blink::WebAXObject* container_obj,
    std::vector<std::unique_ptr<ui::AXNodeData>>* nodes,
    std::map<int32_t, chrome_pdf::PageCharacterIndex>*
        node_id_to_page_char_index,
    std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>*
        node_id_to_annotation_info
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
    PdfOcrHelper* ocr_helper,
    bool has_accessible_text
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    )
    : pdf_accessibility_tree_(std::move(pdf_accessibility_tree)),
      text_runs_(text_runs),
      chars_(chars),
      links_(page_objects.links),
      images_(page_objects.images),
      highlights_(page_objects.highlights),
      text_fields_(page_objects.form_fields.text_fields),
      buttons_(page_objects.form_fields.buttons),
      choice_fields_(page_objects.form_fields.choice_fields),
      page_index_(page_index),
      root_node_(root_node),
      container_obj_(container_obj),
      nodes_(nodes),
      node_id_to_page_char_index_(node_id_to_page_char_index),
      node_id_to_annotation_info_(node_id_to_annotation_info)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      ,
      ocr_helper_(ocr_helper),
      has_accessible_text_(has_accessible_text)
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
{
  page_node_ = CreateAndAppendNode(ax::mojom::Role::kRegion,
                                   ax::mojom::Restriction::kReadOnly);
  page_node_->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetPluralStringFUTF8(IDS_PDF_PAGE_INDEX, page_index + 1));
  page_node_->AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);
  page_node_->relative_bounds.bounds = gfx::RectF(page_info.bounds);
  root_node_->relative_bounds.bounds.Union(page_node_->relative_bounds.bounds);
  root_node_->child_ids.push_back(page_node_->id);

  if (!text_runs.empty()) {
    text_run_start_indices_.reserve(text_runs.size());
    text_run_start_indices_.push_back(0);
    for (size_t i = 0; i < text_runs.size() - 1; ++i) {
      text_run_start_indices_.push_back(text_run_start_indices_[i] +
                                        text_runs[i].len);
    }
  }
}

PdfAccessibilityTreeBuilder::~PdfAccessibilityTreeBuilder() = default;

void PdfAccessibilityTreeBuilder::BuildPageTree() {
  ComputeParagraphAndHeadingThresholds(*text_runs_,
                                       &heading_font_size_threshold_,
                                       &paragraph_spacing_threshold_);

  ui::AXNodeData* para_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  ui::AXNodeData* previous_on_line_node = nullptr;
  std::string static_text;
  LineHelper line_helper(*text_runs_);
  bool pdf_forms_enabled =
      base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm);

  for (size_t text_run_index = 0; text_run_index < text_runs_->size();
       ++text_run_index) {
    // If we don't have a paragraph, create one.
    if (!para_node) {
      para_node =
          CreateParagraphNode((*text_runs_)[text_run_index].style.font_size);
      page_node_->child_ids.push_back(para_node->id);
    }

    // If the `text_run_index` is less than or equal to the link's
    // `text_run_index`, then push the link node in the paragraph.
    if (IsObjectWithRangeInTextRun(*links_, current_link_index_,
                                   text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      const chrome_pdf::AccessibilityLinkInfo& link =
          (*links_)[current_link_index_++];
      AddLinkToParaNode(link, para_node, &previous_on_line_node,
                        &text_run_index);

      if (link.text_range.count == 0) {
        continue;
      }

    } else if (IsObjectInTextRun(*images_, current_image_index_,
                                 text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      AddImageToParaNode((*images_)[current_image_index_++], para_node,
                         &text_run_index);
      continue;
    } else if (IsObjectWithRangeInTextRun(
                   *highlights_, current_highlight_index_, text_run_index)) {
      BuildStaticNode(&static_text_node, &static_text);
      AddHighlightToParaNode((*highlights_)[current_highlight_index_++],
                             para_node, &previous_on_line_node,
                             &text_run_index);
    } else if (IsObjectInTextRun(*text_fields_, current_text_field_index_,
                                 text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddTextFieldToParaNode((*text_fields_)[current_text_field_index_++],
                             para_node, &text_run_index);
      continue;
    } else if (IsObjectInTextRun(*buttons_, current_button_index_,
                                 text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddButtonToParaNode((*buttons_)[current_button_index_++], para_node,
                          &text_run_index);
      continue;
    } else if (IsObjectInTextRun(*choice_fields_, current_choice_field_index_,
                                 text_run_index) &&
               pdf_forms_enabled) {
      BuildStaticNode(&static_text_node, &static_text);
      AddChoiceFieldToParaNode((*choice_fields_)[current_choice_field_index_++],
                               para_node, &text_run_index);
      continue;
    } else {
      chrome_pdf::PageCharacterIndex page_char_index = {
          page_index_, text_run_start_indices_[text_run_index]};

      // This node is for the text inside the paragraph, it includes
      // the text of all of the text runs.
      if (!static_text_node) {
        static_text_node = CreateStaticTextNode(page_char_index);
        para_node->child_ids.push_back(static_text_node->id);
      }

      const chrome_pdf::AccessibilityTextRunInfo& text_run =
          (*text_runs_)[text_run_index];
      // Add this text run to the current static text node.
      ui::AXNodeData* inline_text_box_node =
          CreateInlineTextBoxNode(text_run, page_char_index);
      static_text_node->child_ids.push_back(inline_text_box_node->id);

      static_text += inline_text_box_node->GetStringAttribute(
          ax::mojom::StringAttribute::kName);

      para_node->relative_bounds.bounds.Union(
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

      if (text_run_index < text_runs_->size() - 1) {
        if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
          // The next run is on the same line.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line.
          previous_on_line_node = nullptr;
        }
      }
    }

    if (text_run_index == text_runs_->size() - 1) {
      BuildStaticNode(&static_text_node, &static_text);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(*text_runs_, text_run_index,
                         paragraph_spacing_threshold_)) {
        BuildStaticNode(&static_text_node, &static_text);
        para_node = nullptr;
      }
    }
  }

  AddRemainingAnnotations(para_node);
}

void PdfAccessibilityTreeBuilder::AddWordStartsAndEnds(
    ui::AXNodeData* inline_text_box) {
  std::u16string text =
      inline_text_box->GetString16Attribute(ax::mojom::StringAttribute::kName);
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return;
  }

  std::vector<int32_t> word_starts;
  std::vector<int32_t> word_ends;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_starts.push_back(iter.prev());
      word_ends.push_back(iter.pos());
    }
  }
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                       word_starts);
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                       word_ends);
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateAndAppendNode(
    ax::mojom::Role role,
    ax::mojom::Restriction restriction) {
  std::unique_ptr<ui::AXNodeData> node = std::make_unique<ui::AXNodeData>();
  node->id = container_obj_->GenerateAXID();
  node->role = role;
  node->SetRestriction(restriction);

  // All nodes have coordinates relative to the root node.
  if (root_node_) {
    node->relative_bounds.offset_container_id = root_node_->id;
  }

  ui::AXNodeData* node_ptr = node.get();
  nodes_->push_back(std::move(node));

  return node_ptr;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateParagraphNode(
    float font_size) {
  ui::AXNodeData* para_node = CreateAndAppendNode(
      ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
  para_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                              true);

  // If font size exceeds the `heading_font_size_threshold_`, then classify
  // it as a Heading.
  if (heading_font_size_threshold_ > 0 &&
      font_size > heading_font_size_threshold_) {
    para_node->role = ax::mojom::Role::kHeading;
    para_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 2);
    para_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h2");
  }

  return para_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateStaticTextNode(
    const chrome_pdf::PageCharacterIndex& page_char_index) {
  ui::AXNodeData* static_text_node = CreateAndAppendNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);
  static_text_node->SetNameFrom(ax::mojom::NameFrom::kContents);
  node_id_to_page_char_index_->emplace(static_text_node->id, page_char_index);
  return static_text_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateInlineTextBoxNode(
    const chrome_pdf::AccessibilityTextRunInfo& text_run,
    const chrome_pdf::PageCharacterIndex& page_char_index) {
  ui::AXNodeData* inline_text_box_node = CreateAndAppendNode(
      ax::mojom::Role::kInlineTextBox, ax::mojom::Restriction::kReadOnly);
  inline_text_box_node->SetNameFrom(ax::mojom::NameFrom::kContents);

  std::string chars__utf8 =
      GetTextRunCharsAsUTF8(text_run, *chars_, page_char_index.char_index);
  inline_text_box_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                           chars__utf8);
  inline_text_box_node->AddIntAttribute(
      ax::mojom::IntAttribute::kTextDirection,
      static_cast<uint32_t>(text_run.direction));
  inline_text_box_node->AddStringAttribute(
      ax::mojom::StringAttribute::kFontFamily, text_run.style.font_name);
  inline_text_box_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                          text_run.style.font_size);
  inline_text_box_node->AddFloatAttribute(
      ax::mojom::FloatAttribute::kFontWeight, text_run.style.font_weight);
  if (text_run.style.is_italic) {
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kItalic);
  }
  if (text_run.style.is_bold) {
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kBold);
  }
  if (IsTextRenderModeFill(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.fill_color);
  } else if (IsTextRenderModeStroke(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.stroke_color);
  }

  inline_text_box_node->relative_bounds.bounds =
      text_run.bounds + page_node_->relative_bounds.bounds.OffsetFromOrigin();
  std::vector<int32_t> char_offsets =
      GetTextRunCharOffsets(text_run, *chars_, page_char_index.char_index);
  inline_text_box_node->AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, char_offsets);
  AddWordStartsAndEnds(inline_text_box_node);
  node_id_to_page_char_index_->emplace(inline_text_box_node->id,
                                       page_char_index);
  return inline_text_box_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateLinkNode(
    const chrome_pdf::AccessibilityLinkInfo& link) {
  ui::AXNodeData* link_node = CreateAndAppendNode(
      ax::mojom::Role::kLink, ax::mojom::Restriction::kReadOnly);

  link_node->AddStringAttribute(ax::mojom::StringAttribute::kUrl, link.url);
  link_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                std::string());
  link_node->relative_bounds.bounds = link.bounds;
  link_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kJump);
  node_id_to_annotation_info_->emplace(
      link_node->id,
      PdfAccessibilityTree::AnnotationInfo(page_index_, link.index_in_page));

  return link_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateImageNode(
    const chrome_pdf::AccessibilityImageInfo& image) {
  ui::AXNodeData* image_node = CreateAndAppendNode(
      ax::mojom::Role::kImage, ax::mojom::Restriction::kReadOnly);

  if (image.alt_text.empty()) {
    image_node->AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
  } else {
    image_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   image.alt_text);
  }
  image_node->relative_bounds.bounds = image.bounds;
  return image_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateHighlightNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight) {
  ui::AXNodeData* highlight_node =
      CreateAndAppendNode(ax::mojom::Role::kPdfActionableHighlight,
                          ax::mojom::Restriction::kReadOnly);

  highlight_node->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_HIGHLIGHT));
  highlight_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     std::string());
  highlight_node->relative_bounds.bounds = highlight.bounds;
  highlight_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                  highlight.color);

  return highlight_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreatePopupNoteNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight) {
  ui::AXNodeData* popup_note_node = CreateAndAppendNode(
      ax::mojom::Role::kNote, ax::mojom::Restriction::kReadOnly);

  popup_note_node->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_POPUP_NOTE));
  popup_note_node->relative_bounds.bounds = highlight.bounds;

  ui::AXNodeData* static_popup_note_text_node = CreateAndAppendNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);

  static_popup_note_text_node->SetNameFrom(ax::mojom::NameFrom::kContents);
  static_popup_note_text_node->AddStringAttribute(
      ax::mojom::StringAttribute::kName, highlight.note_text);
  static_popup_note_text_node->relative_bounds.bounds = highlight.bounds;

  popup_note_node->child_ids.push_back(static_popup_note_text_node->id);

  return popup_note_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateTextFieldNode(
    const chrome_pdf::AccessibilityTextFieldInfo& text_field) {
  ax::mojom::Restriction restriction = text_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* text_field_node =
      CreateAndAppendNode(ax::mojom::Role::kTextField, restriction);

  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                      text_field.name);
  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                      text_field.value);
  text_field_node->AddState(ax::mojom::State::kFocusable);
  if (text_field.is_required) {
    text_field_node->AddState(ax::mojom::State::kRequired);
  }
  if (text_field.is_password) {
    text_field_node->AddState(ax::mojom::State::kProtected);
  }
  text_field_node->relative_bounds.bounds = text_field.bounds;
  return text_field_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateButtonNode(
    const chrome_pdf::AccessibilityButtonInfo& button) {
  ax::mojom::Restriction restriction = button.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* button_node =
      CreateAndAppendNode(GetRoleForButtonType(button.type), restriction);
  button_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                  button.name);
  button_node->AddState(ax::mojom::State::kFocusable);

  if (button.type == chrome_pdf::ButtonType::kRadioButton ||
      button.type == chrome_pdf::ButtonType::kCheckBox) {
    ax::mojom::CheckedState checkedState = button.is_checked
                                               ? ax::mojom::CheckedState::kTrue
                                               : ax::mojom::CheckedState::kNone;
    button_node->SetCheckedState(checkedState);
    button_node->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                    button.value);
    button_node->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                                 button.control_count);
    button_node->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                                 button.control_index + 1);
    button_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kCheck);
  } else {
    button_node->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);
  }

  button_node->relative_bounds.bounds = button.bounds;
  return button_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateListboxOptionNode(
    const chrome_pdf::AccessibilityChoiceFieldOptionInfo& choice_field_option,
    ax::mojom::Restriction restriction) {
  ui::AXNodeData* listbox_option_node =
      CreateAndAppendNode(ax::mojom::Role::kListBoxOption, restriction);

  listbox_option_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          choice_field_option.name);
  listbox_option_node->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                        choice_field_option.is_selected);
  listbox_option_node->AddState(ax::mojom::State::kFocusable);
  listbox_option_node->SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kSelect);

  return listbox_option_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateListboxNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ui::AXNodeData* control_node) {
  ax::mojom::Restriction restriction = choice_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* listbox_node =
      CreateAndAppendNode(ax::mojom::Role::kListBox, restriction);

  if (choice_field.type != chrome_pdf::ChoiceFieldType::kComboBox) {
    listbox_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     choice_field.name);
  }

  ui::AXNodeData* first_selected_option = nullptr;
  for (const chrome_pdf::AccessibilityChoiceFieldOptionInfo& option :
       choice_field.options) {
    ui::AXNodeData* listbox_option_node =
        CreateListboxOptionNode(option, restriction);
    if (!first_selected_option && listbox_option_node->GetBoolAttribute(
                                      ax::mojom::BoolAttribute::kSelected)) {
      first_selected_option = listbox_option_node;
    }
    // TODO(crbug.com/40661774): Add `listbox_option_node` specific bounds
    // here.
    listbox_option_node->relative_bounds.bounds = choice_field.bounds;
    listbox_node->child_ids.push_back(listbox_option_node->id);
  }

  if (control_node && first_selected_option) {
    control_node->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  first_selected_option->id);
  }

  if (choice_field.is_multi_select) {
    listbox_node->AddState(ax::mojom::State::kMultiselectable);
  }
  listbox_node->AddState(ax::mojom::State::kFocusable);
  listbox_node->relative_bounds.bounds = choice_field.bounds;
  return listbox_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateComboboxInputNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ax::mojom::Restriction restriction) {
  ax::mojom::Role input_role = choice_field.has_editable_text_box
                                   ? ax::mojom::Role::kTextFieldWithComboBox
                                   : ax::mojom::Role::kComboBoxMenuButton;
  ui::AXNodeData* combobox_input_node =
      CreateAndAppendNode(input_role, restriction);
  combobox_input_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          choice_field.name);
  for (const chrome_pdf::AccessibilityChoiceFieldOptionInfo& option :
       choice_field.options) {
    if (option.is_selected) {
      combobox_input_node->AddStringAttribute(
          ax::mojom::StringAttribute::kValue, option.name);
      break;
    }
  }

  combobox_input_node->AddState(ax::mojom::State::kFocusable);
  combobox_input_node->relative_bounds.bounds = choice_field.bounds;
  if (input_role == ax::mojom::Role::kComboBoxMenuButton) {
    combobox_input_node->SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kOpen);
  }

  return combobox_input_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateComboboxNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
  ax::mojom::Restriction restriction = choice_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* combobox_node =
      CreateAndAppendNode(ax::mojom::Role::kComboBoxGrouping, restriction);
  ui::AXNodeData* input_element =
      CreateComboboxInputNode(choice_field, restriction);
  ui::AXNodeData* list_element = CreateListboxNode(choice_field, input_element);
  input_element->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                     std::vector<int32_t>{list_element->id});
  combobox_node->child_ids.push_back(input_element->id);
  combobox_node->child_ids.push_back(list_element->id);
  combobox_node->AddState(ax::mojom::State::kFocusable);
  combobox_node->relative_bounds.bounds = choice_field.bounds;
  return combobox_node;
}

ui::AXNodeData* PdfAccessibilityTreeBuilder::CreateChoiceFieldNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
  switch (choice_field.type) {
    case chrome_pdf::ChoiceFieldType::kListBox:
      return CreateListboxNode(choice_field, /*control_node=*/nullptr);
    case chrome_pdf::ChoiceFieldType::kComboBox:
      return CreateComboboxNode(choice_field);
  }
}

void PdfAccessibilityTreeBuilder::AddTextToAXNode(
    size_t start_text_run_index,
    uint32_t end_text_run_index,
    ui::AXNodeData* ax_node,
    ui::AXNodeData** previous_on_line_node) {
  chrome_pdf::PageCharacterIndex page_char_index = {
      page_index_, text_run_start_indices_[start_text_run_index]};
  ui::AXNodeData* ax_static_text_node = CreateStaticTextNode(page_char_index);
  ax_node->child_ids.push_back(ax_static_text_node->id);
  // Accumulate the text of the node.
  std::string ax_name;
  LineHelper line_helper(*text_runs_);

  for (size_t text_run_index = start_text_run_index;
       text_run_index <= end_text_run_index; ++text_run_index) {
    const chrome_pdf::AccessibilityTextRunInfo& text_run =
        (*text_runs_)[text_run_index];
    page_char_index.char_index = text_run_start_indices_[text_run_index];
    // Add this text run to the current static text node.
    ui::AXNodeData* inline_text_box_node =
        CreateInlineTextBoxNode(text_run, page_char_index);
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

    if (text_run_index < text_runs_->size() - 1) {
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

void PdfAccessibilityTreeBuilder::AddTextToObjectNode(
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
      std::min(end_text_run_index, text_runs_->size()) - 1;
  AddTextToAXNode(object_text_run_index, object_end_text_run_index, object_node,
                  previous_on_line_node);

  para_node->relative_bounds.bounds.Union(object_node->relative_bounds.bounds);

  *text_run_index =
      NormalizeTextRunIndex(object_end_text_run_index, *text_run_index);
}

void PdfAccessibilityTreeBuilder::AddLinkToParaNode(
    const chrome_pdf::AccessibilityLinkInfo& link,
    ui::AXNodeData* para_node,
    ui::AXNodeData** previous_on_line_node,
    size_t* text_run_index) {
  ui::AXNodeData* link_node = CreateLinkNode(link);
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

void PdfAccessibilityTreeBuilder::AddImageToParaNode(
    const chrome_pdf::AccessibilityImageInfo& image,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the image's text run
  // index, then push the image ahead of the current text run.
  ui::AXNodeData* image_node = CreateImageNode(image);
  para_node->child_ids.push_back(image_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilder::AddHighlightToParaNode(
    const chrome_pdf::AccessibilityHighlightInfo& highlight,
    ui::AXNodeData* para_node,
    ui::AXNodeData** previous_on_line_node,
    size_t* text_run_index) {
  ui::AXNodeData* highlight_node = CreateHighlightNode(highlight);
  para_node->child_ids.push_back(highlight_node->id);

  // Make the text runs contained by the highlight children of
  // the highlight node.
  AddTextToObjectNode(highlight.text_range.index, highlight.text_range.count,
                      highlight_node, para_node, previous_on_line_node,
                      text_run_index);

  if (!highlight.note_text.empty()) {
    ui::AXNodeData* popup_note_node = CreatePopupNoteNode(highlight);
    highlight_node->child_ids.push_back(popup_note_node->id);
  }
}

void PdfAccessibilityTreeBuilder::AddTextFieldToParaNode(
    const chrome_pdf::AccessibilityTextFieldInfo& text_field,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the text_field's text
  // run index, then push the text_field ahead of the current text run.
  ui::AXNodeData* text_field_node = CreateTextFieldNode(text_field);
  para_node->child_ids.push_back(text_field_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilder::AddButtonToParaNode(
    const chrome_pdf::AccessibilityButtonInfo& button,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the button's text
  // run index, then push the button ahead of the current text run.
  ui::AXNodeData* button_node = CreateButtonNode(button);
  para_node->child_ids.push_back(button_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilder::AddChoiceFieldToParaNode(
    const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
    ui::AXNodeData* para_node,
    size_t* text_run_index) {
  // If the `text_run_index` is less than or equal to the choice_field's text
  // run index, then push the choice_field ahead of the current text run.
  ui::AXNodeData* choice_field_node = CreateChoiceFieldNode(choice_field);
  para_node->child_ids.push_back(choice_field_node->id);
  --(*text_run_index);
}

void PdfAccessibilityTreeBuilder::AddRemainingAnnotations(
    ui::AXNodeData* para_node) {
  // If we don't have additional links, images or form fields to insert in the
  // tree, then return.
  if (current_link_index_ >= links_->size() &&
      current_image_index_ >= images_->size() &&
      current_text_field_index_ >= text_fields_->size() &&
      current_button_index_ >= buttons_->size() &&
      current_choice_field_index_ >= choice_fields_->size()) {
    return;
  }

  // If we don't have a paragraph node, create a new one.
  if (!para_node) {
    para_node = CreateAndAppendNode(ax::mojom::Role::kParagraph,
                                    ax::mojom::Restriction::kReadOnly);
    page_node_->child_ids.push_back(para_node->id);
  }
  // Push all the links not anchored to any text run to the last paragraph.
  for (size_t i = current_link_index_; i < links_->size(); i++) {
    ui::AXNodeData* link_node = CreateLinkNode((*links_)[i]);
    para_node->child_ids.push_back(link_node->id);
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  base::queue<PdfOcrRequest> ocr_requests;
  bool ocr_available = ocr_helper_;
#endif

  // Push all the images not anchored to any text run to the last paragraph.
  for (size_t i = current_image_index_; i < images_->size(); i++) {
    const chrome_pdf::AccessibilityImageInfo& image_info = (*images_)[i];
    ui::AXNodeData* image_node = CreateImageNode(image_info);
    para_node->child_ids.push_back(image_node->id);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    if (!has_accessible_text_ && ocr_available) {
      if (image_info.alt_text.empty()) {
        image_node->SetNameChecked(l10n_util::GetStringUTF8(
            IDS_PDF_OCR_IN_PROGRESS_AX_UNLABELED_IMAGE));
      }
      ocr_requests.emplace(image_node->id, image_info, root_node_->id,
                           para_node->id, page_node_->id, page_index_);
    }
#endif
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (!ocr_requests.empty()) {
    ocr_helper_->OcrPage(std::move(ocr_requests));
  }
#endif

  if (base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm)) {
    // Push all the text fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_text_field_index_; i < text_fields_->size(); i++) {
      ui::AXNodeData* text_field_node = CreateTextFieldNode((*text_fields_)[i]);
      para_node->child_ids.push_back(text_field_node->id);
    }

    // Push all the buttons not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_button_index_; i < buttons_->size(); i++) {
      ui::AXNodeData* button_node = CreateButtonNode((*buttons_)[i]);
      para_node->child_ids.push_back(button_node->id);
    }

    // Push all the choice fields not anchored to any text run to the last
    // paragraph.
    for (size_t i = current_choice_field_index_; i < choice_fields_->size();
         i++) {
      ui::AXNodeData* choice_field_node =
          CreateChoiceFieldNode((*choice_fields_)[i]);
      para_node->child_ids.push_back(choice_field_node->id);
    }
  }
}

}  // namespace pdf
