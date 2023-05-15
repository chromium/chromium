// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/break_iterator.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "components/pdf/renderer/pdf_ax_action_target.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_action_handler.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/strings/grit/blink_accessibility_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "ui/accessibility/accessibility_features.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace pdf {

namespace ranges = base::ranges;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// Handles the connection to the Screen AI Service which can perform OCR on
// images.
class PdfOcrService final {
 public:
  using OcrServiceCallback = screen_ai::mojom::ScreenAIAnnotator::
      PerformOcrAndReturnAXTreeUpdateCallback;

  using OnOcrDataReceivedCallback = base::RepeatingCallback<void(
      const ui::AXNodeID& image_node_id,
      const chrome_pdf::AccessibilityImageInfo& image,
      const ui::AXNodeID& parent_node_id,
      const ui::AXTreeUpdate& tree_update)>;

  struct OcrRequest {
    OcrRequest(const ui::AXNodeID& image_node_id,
               const chrome_pdf::AccessibilityImageInfo& image,
               const ui::AXNodeID& parent_node_id)
        : image_node_id(image_node_id),
          image(image),
          parent_node_id(parent_node_id) {}

    const ui::AXNodeID image_node_id;
    const chrome_pdf::AccessibilityImageInfo image;
    const ui::AXNodeID parent_node_id;
  };

  PdfOcrService(content::RenderFrame& render_frame,
                OnOcrDataReceivedCallback callback)
      : callback_(std::move(callback)) {
    if (features::IsPdfOcrEnabled()) {
      render_frame.GetBrowserInterfaceBroker()->GetInterface(
          screen_ai_annotator_.BindNewPipeAndPassReceiver());
    }
  }

  PdfOcrService(const PdfOcrService&) = delete;
  PdfOcrService& operator=(const PdfOcrService&) = delete;
  ~PdfOcrService() = default;

  // Schedules the OCR requests to be sent to the Screen AI Service.
  void ScheduleOcrRequests(base::queue<OcrRequest> requests) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(IsOcrReady());

    if (queued_requests_.empty()) {
      queued_requests_.swap(requests);
    } else {
      while (!requests.empty()) {
        queued_requests_.push(requests.front());
        requests.pop();
      }
    }

    ScheduleNextQueuedTask();
  }

  bool IsOcrReady() const { return screen_ai_annotator_.is_bound(); }

  bool IsQueueEmpty() const { return queued_requests_.empty(); }

 private:
  void ScheduleNextQueuedTask() {
    if (queued_requests_.empty()) {
      return;
    }

    OcrRequest request = queued_requests_.front();
    queued_requests_.pop();

    screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
        request.image.image_data,
        base::BindOnce(&PdfOcrService::ReceiveOcrResultsForRequest,
                       weak_ptr_factory_.GetWeakPtr(), request));
  }

  void ReceiveOcrResultsForRequest(OcrRequest request,
                                   const ui::AXTreeUpdate& tree_update) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    callback_.Run(request.image_node_id, request.image, request.parent_node_id,
                  tree_update);

    ScheduleNextQueuedTask();
  }

  base::queue<OcrRequest> queued_requests_;
  OnOcrDataReceivedCallback callback_;

  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PdfOcrService> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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
    DCHECK(current_index == 0 || current_index < text_runs_.size());
    start_index_ = current_index;
    accumulated_weight_top_ = 0.0f;
    accumulated_weight_bottom_ = 0.0f;
    accumulated_width_ = 0.0f;
  }

  void ProcessNextRun(size_t run_index) {
    DCHECK_LT(run_index, text_runs_.size());
    RemoveOldRunsUpTo(run_index);
    AddRun(text_runs_[run_index].bounds);
  }

  bool IsRunOnSameLine(size_t run_index) const {
    DCHECK_LT(run_index, text_runs_.size());

    // Calculate new top/bottom bounds for our line.
    if (accumulated_width_ == 0.0f)
      return false;

    float line_top = accumulated_weight_top_ / accumulated_width_;
    float line_bottom = accumulated_weight_bottom_ / accumulated_width_;

    // Look at the next run, and determine how much it overlaps the line.
    const auto& run_bounds = text_runs_[run_index].bounds;
    if (run_bounds.height() == 0.0f)
      return false;

    float clamped_top = std::max(line_top, run_bounds.y());
    float clamped_bottom =
        std::min(line_bottom, run_bounds.y() + run_bounds.height());
    if (clamped_bottom < clamped_top)
      return false;

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
           accumulated_width_ > text_runs_[start_index_].bounds.width() *
                                    kBoxRemoveWidthThreshold) {
      const auto& old_bounds = text_runs_[start_index_].bounds;
      RemoveRun(old_bounds);
      start_index_++;
    }
  }

  const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs_;
  size_t start_index_;
  float accumulated_weight_top_;
  float accumulated_weight_bottom_;
  float accumulated_width_;
};

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
      if (cur.y() > prev.y() + prev.height() / 2)
        line_spacings.push_back(cur.y() - prev.y());
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

ui::AXNode* GetStaticTextNodeFromNode(ui::AXNode* node) {
  // Returns the appropriate static text node given `node`'s type.
  // Returns nullptr if there is no appropriate static text node.
  if (!node)
    return nullptr;
  ui::AXNode* static_node = node;
  // Get the static text from the link node.
  if (node->GetRole() == ax::mojom::Role::kLink &&
      node->children().size() == 1) {
    static_node = node->children()[0];
  }
  // Get the static text from the highlight node.
  if (node->GetRole() == ax::mojom::Role::kPdfActionableHighlight &&
      !node->children().empty()) {
    static_node = node->children()[0];
  }
  // If it's static text node, then it holds text.
  if (static_node && static_node->GetRole() == ax::mojom::Role::kStaticText)
    return static_node;
  return nullptr;
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
bool CompareTextRuns(const T& a, const T& b) {
  return a.text_run_index < b.text_run_index;
}

template <typename T>
bool CompareTextRunsWithRange(const T& a, const T& b) {
  return a.text_range.index < b.text_range.index;
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

std::unique_ptr<ui::AXNodeData> CreateNode(
    ax::mojom::Role role,
    ax::mojom::Restriction restriction,
    content::RenderAccessibility* render_accessibility) {
  auto node = std::make_unique<ui::AXNodeData>();
  node->id = render_accessibility->GenerateAXID();
  node->role = role;
  node->SetRestriction(restriction);

  return node;
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// TODO(crbug.com/1393069): Need to test this status node with screen readers
// on other desktop platforms, such as Windows, macOS, and Linux, as well as in
// the embedded PDF case.
std::unique_ptr<ui::AXNodeData> CreateStatusNode(
    content::RenderAccessibility* render_accessibility,
    ui::AXNodeData* root_node) {
  // Create a status node that conveys a notification message and add it under
  // the PDF root node as the first node.
  std::unique_ptr<ui::AXNodeData> node =
      CreateNode(ax::mojom::Role::kStatus, ax::mojom::Restriction::kReadOnly,
                 render_accessibility);
  // Set the origin of this status node to be offscreen with a 1x1 rectangle as
  // this status node doesn't have a visual element. The origin of the doc is
  // (0, 0), so setting (-1, -1) will make this node offscreen.
  node->relative_bounds.bounds = gfx::RectF(-1, -1, 1, 1);
  node->relative_bounds.offset_container_id = root_node->id;
  // As we create this status node right after the PDF root node, this node
  // will be added as the first node to the PDF accessibility tree.
  CHECK(root_node->child_ids.empty());
  root_node->child_ids.push_back(node->id);
  VLOG(2) << "Creating an OCR status node.";
  return node;
}

gfx::Transform MakeTransformForImage(
    const chrome_pdf::AccessibilityImageInfo& image) {
  // Nodes created with OCR results from the image will be misaligned on screen
  // if `image_screen_size` is different from `image_pixel_size`. To address
  // this misalignment issue, an additional transform needs to be created.
  const gfx::RectF& image_screen_size = image.bounds;
  const gfx::RectF image_pixel_size =
      gfx::RectF(image.image_data.width(), image.image_data.height());
  CHECK(!image_pixel_size.IsEmpty());

  gfx::Transform transform;
  float width_scale_factor =
      image_screen_size.width() / image_pixel_size.width();
  float height_scale_factor =
      image_screen_size.height() / image_pixel_size.height();
  transform.Scale(width_scale_factor, height_scale_factor);

  return transform;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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

class PdfAccessibilityTreeBuilder {
 public:
  PdfAccessibilityTreeBuilder(
      base::WeakPtr<PdfAccessibilityTree> pdf_accessibility_tree,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects,
      const chrome_pdf::AccessibilityPageInfo& page_info,
      uint32_t page_index,
      ui::AXNodeData* root_node,
      content::RenderAccessibility* render_accessibility,
      std::vector<std::unique_ptr<ui::AXNodeData>>* nodes,
      std::map<int32_t, chrome_pdf::PageCharacterIndex>*
          node_id_to_page_char_index,
      std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>*
          node_id_to_annotation_info
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      ,
      PdfOcrService* ocr_service,
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
        render_accessibility_(render_accessibility),
        nodes_(nodes),
        node_id_to_page_char_index_(node_id_to_page_char_index),
        node_id_to_annotation_info_(node_id_to_annotation_info)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
        ,
        ocr_service_(ocr_service),
        has_accessible_text_(has_accessible_text)
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  {
    page_node_ = CreateAndAppendNode(ax::mojom::Role::kRegion,
                                     ax::mojom::Restriction::kReadOnly);
    page_node_->AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        l10n_util::GetPluralStringFUTF8(IDS_PDF_PAGE_INDEX, page_index + 1));
    page_node_->AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);
    page_node_->relative_bounds.bounds = gfx::RectF(page_info.bounds);
    root_node_->relative_bounds.bounds.Union(
        page_node_->relative_bounds.bounds);
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

  PdfAccessibilityTreeBuilder(const PdfAccessibilityTreeBuilder&) = delete;
  PdfAccessibilityTreeBuilder& operator=(const PdfAccessibilityTreeBuilder&) =
      delete;
  ~PdfAccessibilityTreeBuilder() = default;

  void BuildPageTree() {
    ComputeParagraphAndHeadingThresholds(text_runs_,
                                         &heading_font_size_threshold_,
                                         &paragraph_spacing_threshold_);

    ui::AXNodeData* para_node = nullptr;
    ui::AXNodeData* static_text_node = nullptr;
    ui::AXNodeData* previous_on_line_node = nullptr;
    std::string static_text;
    LineHelper line_helper(text_runs_);
    bool pdf_forms_enabled =
        base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm);

    for (size_t text_run_index = 0; text_run_index < text_runs_.size();
         ++text_run_index) {
      // If we don't have a paragraph, create one.
      if (!para_node) {
        para_node =
            CreateParagraphNode(text_runs_[text_run_index].style.font_size);
        page_node_->child_ids.push_back(para_node->id);
      }

      // If the `text_run_index` is less than or equal to the link's
      // `text_run_index`, then push the link node in the paragraph.
      if (IsObjectWithRangeInTextRun(links_, current_link_index_,
                                     text_run_index)) {
        BuildStaticNode(&static_text_node, &static_text);
        const chrome_pdf::AccessibilityLinkInfo& link =
            links_[current_link_index_++];
        AddLinkToParaNode(link, para_node, &previous_on_line_node,
                          &text_run_index);

        if (link.text_range.count == 0)
          continue;

      } else if (IsObjectInTextRun(images_, current_image_index_,
                                   text_run_index)) {
        BuildStaticNode(&static_text_node, &static_text);
        AddImageToParaNode(images_[current_image_index_++], para_node,
                           &text_run_index);
        continue;
      } else if (IsObjectWithRangeInTextRun(
                     highlights_, current_highlight_index_, text_run_index)) {
        BuildStaticNode(&static_text_node, &static_text);
        AddHighlightToParaNode(highlights_[current_highlight_index_++],
                               para_node, &previous_on_line_node,
                               &text_run_index);
      } else if (IsObjectInTextRun(text_fields_, current_text_field_index_,
                                   text_run_index) &&
                 pdf_forms_enabled) {
        BuildStaticNode(&static_text_node, &static_text);
        AddTextFieldToParaNode(text_fields_[current_text_field_index_++],
                               para_node, &text_run_index);
        continue;
      } else if (IsObjectInTextRun(buttons_, current_button_index_,
                                   text_run_index) &&
                 pdf_forms_enabled) {
        BuildStaticNode(&static_text_node, &static_text);
        AddButtonToParaNode(buttons_[current_button_index_++], para_node,
                            &text_run_index);
        continue;
      } else if (IsObjectInTextRun(choice_fields_, current_choice_field_index_,
                                   text_run_index) &&
                 pdf_forms_enabled) {
        BuildStaticNode(&static_text_node, &static_text);
        AddChoiceFieldToParaNode(choice_fields_[current_choice_field_index_++],
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
            text_runs_[text_run_index];
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

        if (text_run_index < text_runs_.size() - 1) {
          if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
            // The next run is on the same line.
            previous_on_line_node = inline_text_box_node;
          } else {
            // The next run is on a new line.
            previous_on_line_node = nullptr;
          }
        }
      }

      if (text_run_index == text_runs_.size() - 1) {
        BuildStaticNode(&static_text_node, &static_text);
        break;
      }

      if (!previous_on_line_node) {
        if (BreakParagraph(text_runs_, text_run_index,
                           paragraph_spacing_threshold_)) {
          BuildStaticNode(&static_text_node, &static_text);
          para_node = nullptr;
        }
      }
    }

    AddRemainingAnnotations(para_node);
  }

 private:
  void AddWordStartsAndEnds(ui::AXNodeData* inline_text_box) {
    std::u16string text = inline_text_box->GetString16Attribute(
        ax::mojom::StringAttribute::kName);
    base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
    if (!iter.Init())
      return;

    std::vector<int32_t> word_starts;
    std::vector<int32_t> word_ends;
    while (iter.Advance()) {
      if (iter.IsWord()) {
        word_starts.push_back(iter.prev());
        word_ends.push_back(iter.pos());
      }
    }
    inline_text_box->AddIntListAttribute(
        ax::mojom::IntListAttribute::kWordStarts, word_starts);
    inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                         word_ends);
  }

  ui::AXNodeData* CreateAndAppendNode(ax::mojom::Role role,
                                      ax::mojom::Restriction restriction) {
    std::unique_ptr<ui::AXNodeData> node =
        CreateNode(role, restriction, render_accessibility_);

    // All nodes have coordinates relative to the root node.
    if (root_node_) {
      node->relative_bounds.offset_container_id = root_node_->id;
    }

    ui::AXNodeData* node_ptr = node.get();
    nodes_->push_back(std::move(node));

    return node_ptr;
  }

  ui::AXNodeData* CreateParagraphNode(float font_size) {
    ui::AXNodeData* para_node = CreateAndAppendNode(
        ax::mojom::Role::kParagraph, ax::mojom::Restriction::kReadOnly);
    para_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                                true);

    // If font size exceeds the `heading_font_size_threshold_`, then classify
    // it as a Heading.
    if (heading_font_size_threshold_ > 0 &&
        font_size > heading_font_size_threshold_) {
      para_node->role = ax::mojom::Role::kHeading;
      para_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                 2);
      para_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h2");
    }

    return para_node;
  }

  ui::AXNodeData* CreateStaticTextNode(
      const chrome_pdf::PageCharacterIndex& page_char_index) {
    ui::AXNodeData* static_text_node = CreateAndAppendNode(
        ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);
    static_text_node->SetNameFrom(ax::mojom::NameFrom::kContents);
    node_id_to_page_char_index_->emplace(static_text_node->id, page_char_index);
    return static_text_node;
  }

  ui::AXNodeData* CreateInlineTextBoxNode(
      const chrome_pdf::AccessibilityTextRunInfo& text_run,
      const chrome_pdf::PageCharacterIndex& page_char_index) {
    ui::AXNodeData* inline_text_box_node = CreateAndAppendNode(
        ax::mojom::Role::kInlineTextBox, ax::mojom::Restriction::kReadOnly);
    inline_text_box_node->SetNameFrom(ax::mojom::NameFrom::kContents);

    std::string chars__utf8 =
        GetTextRunCharsAsUTF8(text_run, chars_, page_char_index.char_index);
    inline_text_box_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                             chars__utf8);
    inline_text_box_node->AddIntAttribute(
        ax::mojom::IntAttribute::kTextDirection,
        static_cast<uint32_t>(text_run.direction));
    inline_text_box_node->AddStringAttribute(
        ax::mojom::StringAttribute::kFontFamily, text_run.style.font_name);
    inline_text_box_node->AddFloatAttribute(
        ax::mojom::FloatAttribute::kFontSize, text_run.style.font_size);
    inline_text_box_node->AddFloatAttribute(
        ax::mojom::FloatAttribute::kFontWeight, text_run.style.font_weight);
    if (text_run.style.is_italic)
      inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kItalic);
    if (text_run.style.is_bold)
      inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kBold);
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
        GetTextRunCharOffsets(text_run, chars_, page_char_index.char_index);
    inline_text_box_node->AddIntListAttribute(
        ax::mojom::IntListAttribute::kCharacterOffsets, char_offsets);
    AddWordStartsAndEnds(inline_text_box_node);
    node_id_to_page_char_index_->emplace(inline_text_box_node->id,
                                         page_char_index);
    return inline_text_box_node;
  }

  ui::AXNodeData* CreateLinkNode(
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

  ui::AXNodeData* CreateImageNode(
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

  ui::AXNodeData* CreateHighlightNode(
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

  ui::AXNodeData* CreatePopupNoteNode(
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

  ui::AXNodeData* CreateTextFieldNode(
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
    if (text_field.is_required)
      text_field_node->AddState(ax::mojom::State::kRequired);
    if (text_field.is_password)
      text_field_node->AddState(ax::mojom::State::kProtected);
    text_field_node->relative_bounds.bounds = text_field.bounds;
    return text_field_node;
  }

  ui::AXNodeData* CreateButtonNode(
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
      ax::mojom::CheckedState checkedState =
          button.is_checked ? ax::mojom::CheckedState::kTrue
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

  ui::AXNodeData* CreateListboxOptionNode(
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

  ui::AXNodeData* CreateListboxNode(
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
      // TODO(crbug.com/1030242): Add `listbox_option_node` specific bounds
      // here.
      listbox_option_node->relative_bounds.bounds = choice_field.bounds;
      listbox_node->child_ids.push_back(listbox_option_node->id);
    }

    if (control_node && first_selected_option) {
      control_node->AddIntAttribute(
          ax::mojom::IntAttribute::kActivedescendantId,
          first_selected_option->id);
    }

    if (choice_field.is_multi_select)
      listbox_node->AddState(ax::mojom::State::kMultiselectable);
    listbox_node->AddState(ax::mojom::State::kFocusable);
    listbox_node->relative_bounds.bounds = choice_field.bounds;
    return listbox_node;
  }

  ui::AXNodeData* CreateComboboxInputNode(
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

  ui::AXNodeData* CreateComboboxNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
    ax::mojom::Restriction restriction = choice_field.is_read_only
                                             ? ax::mojom::Restriction::kReadOnly
                                             : ax::mojom::Restriction::kNone;
    ui::AXNodeData* combobox_node =
        CreateAndAppendNode(ax::mojom::Role::kComboBoxGrouping, restriction);
    ui::AXNodeData* input_element =
        CreateComboboxInputNode(choice_field, restriction);
    ui::AXNodeData* list_element =
        CreateListboxNode(choice_field, input_element);
    input_element->AddIntListAttribute(
        ax::mojom::IntListAttribute::kControlsIds,
        std::vector<int32_t>{list_element->id});
    combobox_node->child_ids.push_back(input_element->id);
    combobox_node->child_ids.push_back(list_element->id);
    combobox_node->AddState(ax::mojom::State::kFocusable);
    combobox_node->relative_bounds.bounds = choice_field.bounds;
    return combobox_node;
  }

  ui::AXNodeData* CreateChoiceFieldNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field) {
    switch (choice_field.type) {
      case chrome_pdf::ChoiceFieldType::kListBox:
        return CreateListboxNode(choice_field, /*control_node=*/nullptr);
      case chrome_pdf::ChoiceFieldType::kComboBox:
        return CreateComboboxNode(choice_field);
    }
  }

  void AddTextToAXNode(size_t start_text_run_index,
                       uint32_t end_text_run_index,
                       ui::AXNodeData* ax_node,
                       ui::AXNodeData** previous_on_line_node) {
    chrome_pdf::PageCharacterIndex page_char_index = {
        page_index_, text_run_start_indices_[start_text_run_index]};
    ui::AXNodeData* ax_static_text_node = CreateStaticTextNode(page_char_index);
    ax_node->child_ids.push_back(ax_static_text_node->id);
    // Accumulate the text of the node.
    std::string ax_name;
    LineHelper line_helper(text_runs_);

    for (size_t text_run_index = start_text_run_index;
         text_run_index <= end_text_run_index; ++text_run_index) {
      const chrome_pdf::AccessibilityTextRunInfo& text_run =
          text_runs_[text_run_index];
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

      if (text_run_index < text_runs_.size() - 1) {
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

  void AddTextToObjectNode(size_t object_text_run_index,
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
        std::min(end_text_run_index, text_runs_.size()) - 1;
    AddTextToAXNode(object_text_run_index, object_end_text_run_index,
                    object_node, previous_on_line_node);

    para_node->relative_bounds.bounds.Union(
        object_node->relative_bounds.bounds);

    *text_run_index =
        NormalizeTextRunIndex(object_end_text_run_index, *text_run_index);
  }

  void AddLinkToParaNode(const chrome_pdf::AccessibilityLinkInfo& link,
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

  void AddImageToParaNode(const chrome_pdf::AccessibilityImageInfo& image,
                          ui::AXNodeData* para_node,
                          size_t* text_run_index) {
    // If the `text_run_index` is less than or equal to the image's text run
    // index, then push the image ahead of the current text run.
    ui::AXNodeData* image_node = CreateImageNode(image);
    para_node->child_ids.push_back(image_node->id);
    --(*text_run_index);
  }

  void AddHighlightToParaNode(
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

  void AddTextFieldToParaNode(
      const chrome_pdf::AccessibilityTextFieldInfo& text_field,
      ui::AXNodeData* para_node,
      size_t* text_run_index) {
    // If the `text_run_index` is less than or equal to the text_field's text
    // run index, then push the text_field ahead of the current text run.
    ui::AXNodeData* text_field_node = CreateTextFieldNode(text_field);
    para_node->child_ids.push_back(text_field_node->id);
    --(*text_run_index);
  }

  void AddButtonToParaNode(const chrome_pdf::AccessibilityButtonInfo& button,
                           ui::AXNodeData* para_node,
                           size_t* text_run_index) {
    // If the `text_run_index` is less than or equal to the button's text
    // run index, then push the button ahead of the current text run.
    ui::AXNodeData* button_node = CreateButtonNode(button);
    para_node->child_ids.push_back(button_node->id);
    --(*text_run_index);
  }

  void AddChoiceFieldToParaNode(
      const chrome_pdf::AccessibilityChoiceFieldInfo& choice_field,
      ui::AXNodeData* para_node,
      size_t* text_run_index) {
    // If the `text_run_index` is less than or equal to the choice_field's text
    // run index, then push the choice_field ahead of the current text run.
    ui::AXNodeData* choice_field_node = CreateChoiceFieldNode(choice_field);
    para_node->child_ids.push_back(choice_field_node->id);
    --(*text_run_index);
  }

  void AddRemainingAnnotations(ui::AXNodeData* para_node) {
    // If we don't have additional links, images or form fields to insert in the
    // tree, then return.
    if (current_link_index_ >= links_.size() &&
        current_image_index_ >= images_.size() &&
        current_text_field_index_ >= text_fields_.size() &&
        current_button_index_ >= buttons_.size() &&
        current_choice_field_index_ >= choice_fields_.size()) {
      return;
    }

    // If we don't have a paragraph node, create a new one.
    if (!para_node) {
      para_node = CreateAndAppendNode(ax::mojom::Role::kParagraph,
                                      ax::mojom::Restriction::kReadOnly);
      page_node_->child_ids.push_back(para_node->id);
    }
    // Push all the links not anchored to any text run to the last paragraph.
    for (size_t i = current_link_index_; i < links_.size(); i++) {
      ui::AXNodeData* link_node = CreateLinkNode(links_[i]);
      para_node->child_ids.push_back(link_node->id);
    }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    base::queue<PdfOcrService::OcrRequest> ocr_requests;
    bool ocr_available = ocr_service_ && ocr_service_->IsOcrReady();
#endif

    // Push all the images not anchored to any text run to the last paragraph.
    for (size_t i = current_image_index_; i < images_.size(); i++) {
      ui::AXNodeData* image_node = CreateImageNode(images_[i]);
      para_node->child_ids.push_back(image_node->id);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      // TODO(crbug.com/1278249): Consider moving images instead of copying
      // them.
      if (!has_accessible_text_ && ocr_available &&
          !images_[i].image_data.drawsNothing()) {
        ocr_requests.emplace(image_node->id, images_[i], para_node->id);
      }
#endif
    }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    if (!ocr_requests.empty()) {
      ocr_service_->ScheduleOcrRequests(std::move(ocr_requests));
    }
#endif

    if (base::FeatureList::IsEnabled(
            chrome_pdf::features::kAccessiblePDFForm)) {
      // Push all the text fields not anchored to any text run to the last
      // paragraph.
      for (size_t i = current_text_field_index_; i < text_fields_.size(); i++) {
        ui::AXNodeData* text_field_node = CreateTextFieldNode(text_fields_[i]);
        para_node->child_ids.push_back(text_field_node->id);
      }

      // Push all the buttons not anchored to any text run to the last
      // paragraph.
      for (size_t i = current_button_index_; i < buttons_.size(); i++) {
        ui::AXNodeData* button_node = CreateButtonNode(buttons_[i]);
        para_node->child_ids.push_back(button_node->id);
      }

      // Push all the choice fields not anchored to any text run to the last
      // paragraph.
      for (size_t i = current_choice_field_index_; i < choice_fields_.size();
           i++) {
        ui::AXNodeData* choice_field_node =
            CreateChoiceFieldNode(choice_fields_[i]);
        para_node->child_ids.push_back(choice_field_node->id);
      }
    }
  }

  base::WeakPtr<PdfAccessibilityTree> pdf_accessibility_tree_;
  std::vector<uint32_t> text_run_start_indices_;
  const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs_;
  const std::vector<chrome_pdf::AccessibilityCharInfo>& chars_;
  const std::vector<chrome_pdf::AccessibilityLinkInfo>& links_;
  uint32_t current_link_index_ = 0;
  const std::vector<chrome_pdf::AccessibilityImageInfo>& images_;
  uint32_t current_image_index_ = 0;
  const std::vector<chrome_pdf::AccessibilityHighlightInfo>& highlights_;
  uint32_t current_highlight_index_ = 0;
  const std::vector<chrome_pdf::AccessibilityTextFieldInfo>& text_fields_;
  uint32_t current_text_field_index_ = 0;
  const std::vector<chrome_pdf::AccessibilityButtonInfo>& buttons_;
  uint32_t current_button_index_ = 0;
  const std::vector<chrome_pdf::AccessibilityChoiceFieldInfo>& choice_fields_;
  uint32_t current_choice_field_index_ = 0;
  uint32_t page_index_;
  ui::AXNodeData* root_node_;
  ui::AXNodeData* page_node_;
  content::RenderAccessibility* render_accessibility_;
  std::vector<std::unique_ptr<ui::AXNodeData>>* nodes_;
  std::map<int32_t, chrome_pdf::PageCharacterIndex>*
      node_id_to_page_char_index_;
  std::map<int32_t, PdfAccessibilityTree::AnnotationInfo>*
      node_id_to_annotation_info_;
  float heading_font_size_threshold_ = 0;
  float paragraph_spacing_threshold_ = 0;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  PdfOcrService* ocr_service_ = nullptr;
  const bool has_accessible_text_;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace

PdfAccessibilityTree::PdfAccessibilityTree(
    content::RenderFrame* render_frame,
    chrome_pdf::PdfAccessibilityActionHandler* action_handler)
    : content::RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      action_handler_(action_handler) {
  DCHECK(render_frame);
  DCHECK(action_handler_);
  MaybeHandleAccessibilityChange(/*always_load_or_reload_accessibility=*/false);
}

PdfAccessibilityTree::~PdfAccessibilityTree() {
  // Even if `render_accessibility` is disabled, still let it know `this` is
  // being destroyed.
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility)
    render_accessibility->SetPluginTreeSource(nullptr);
}

// static
bool PdfAccessibilityTree::IsDataFromPluginValid(
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects) {
  base::CheckedNumeric<uint32_t> char_length = 0;
  for (const chrome_pdf::AccessibilityTextRunInfo& text_run : text_runs)
    char_length += text_run.len;

  if (!char_length.IsValid() || char_length.ValueOrDie() != chars.size())
    return false;

  const std::vector<chrome_pdf::AccessibilityLinkInfo>& links =
      page_objects.links;
  if (!std::is_sorted(
          links.begin(), links.end(),
          CompareTextRunsWithRange<chrome_pdf::AccessibilityLinkInfo>)) {
    return false;
  }
  // Text run index of a `link` is out of bounds if it exceeds the size of
  // `text_runs`. The index denotes the position of the link relative to the
  // text runs. The index value equal to the size of `text_runs` indicates that
  // the link should be after the last text run.
  // `index_in_page` of every `link` should be with in the range of total number
  // of links, which is size of `links`.
  for (const chrome_pdf::AccessibilityLinkInfo& link : links) {
    base::CheckedNumeric<size_t> index = link.text_range.index;
    index += link.text_range.count;
    if (!index.IsValid() || index.ValueOrDie() > text_runs.size() ||
        link.index_in_page >= links.size()) {
      return false;
    }
  }

  const std::vector<chrome_pdf::AccessibilityImageInfo>& images =
      page_objects.images;
  if (!std::is_sorted(images.begin(), images.end(),
                      CompareTextRuns<chrome_pdf::AccessibilityImageInfo>)) {
    return false;
  }
  // Text run index of an `image` works on the same logic as the text run index
  // of a `link` as described above.
  for (const chrome_pdf::AccessibilityImageInfo& image : images) {
    if (image.text_run_index > text_runs.size())
      return false;
  }

  const std::vector<chrome_pdf::AccessibilityHighlightInfo>& highlights =
      page_objects.highlights;
  if (!std::is_sorted(
          highlights.begin(), highlights.end(),
          CompareTextRunsWithRange<chrome_pdf::AccessibilityHighlightInfo>)) {
    return false;
  }

  // Since highlights also span across text runs similar to links, the
  // validation method is the same.
  // `index_in_page` of a `highlight` follows the same index validation rules
  // as of links.
  for (const auto& highlight : highlights) {
    base::CheckedNumeric<size_t> index = highlight.text_range.index;
    index += highlight.text_range.count;
    if (!index.IsValid() || index.ValueOrDie() > text_runs.size() ||
        highlight.index_in_page >= highlights.size()) {
      return false;
    }
  }

  const std::vector<chrome_pdf::AccessibilityTextFieldInfo>& text_fields =
      page_objects.form_fields.text_fields;
  if (!std::is_sorted(
          text_fields.begin(), text_fields.end(),
          CompareTextRuns<chrome_pdf::AccessibilityTextFieldInfo>)) {
    return false;
  }
  // Text run index of an `text_field` works on the same logic as the text run
  // index of a `link` as mentioned above.
  // `index_in_page` of a `text_field` follows the same index validation rules
  // as of links.
  for (const chrome_pdf::AccessibilityTextFieldInfo& text_field : text_fields) {
    if (text_field.text_run_index > text_runs.size() ||
        text_field.index_in_page >= text_fields.size()) {
      return false;
    }
  }

  const std::vector<chrome_pdf::AccessibilityChoiceFieldInfo>& choice_fields =
      page_objects.form_fields.choice_fields;
  if (!std::is_sorted(
          choice_fields.begin(), choice_fields.end(),
          CompareTextRuns<chrome_pdf::AccessibilityChoiceFieldInfo>)) {
    return false;
  }
  for (const auto& choice_field : choice_fields) {
    // Text run index of an `choice_field` works on the same logic as the text
    // run index of a `link` as mentioned above.
    // `index_in_page` of a `choice_field` follows the same index validation
    // rules as of links.
    if (choice_field.text_run_index > text_runs.size() ||
        choice_field.index_in_page >= choice_fields.size()) {
      return false;
    }

    // The type should be valid.
    if (choice_field.type < chrome_pdf::ChoiceFieldType::kMinValue ||
        choice_field.type > chrome_pdf::ChoiceFieldType::kMaxValue) {
      return false;
    }
  }

  const std::vector<chrome_pdf::AccessibilityButtonInfo>& buttons =
      page_objects.form_fields.buttons;
  if (!std::is_sorted(buttons.begin(), buttons.end(),
                      CompareTextRuns<chrome_pdf::AccessibilityButtonInfo>)) {
    return false;
  }
  for (const chrome_pdf::AccessibilityButtonInfo& button : buttons) {
    // Text run index of an `button` works on the same logic as the text run
    // index of a `link` as mentioned above.
    // `index_in_page` of a `button` follows the same index validation rules as
    // of links.
    if (button.text_run_index > text_runs.size() ||
        button.index_in_page >= buttons.size()) {
      return false;
    }

    // The type should be valid.
    if (button.type < chrome_pdf::ButtonType::kMinValue ||
        button.type > chrome_pdf::ButtonType::kMaxValue) {
      return false;
    }

    // For radio button or checkbox, value of `button.control_index` should
    // always be less than `button.control_count`.
    if ((button.type == chrome_pdf::ButtonType::kCheckBox ||
         button.type == chrome_pdf::ButtonType::kRadioButton) &&
        (button.control_index >= button.control_count)) {
      return false;
    }
  }

  return true;
}

void PdfAccessibilityTree::SetAccessibilityViewportInfo(
    chrome_pdf::AccessibilityViewportInfo viewport_info) {
  // This call may trigger layout, and ultimately self-deletion; see
  // crbug.com/1274376 for details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfAccessibilityTree::DoSetAccessibilityViewportInfo,
                     GetWeakPtr(), std::move(viewport_info)));
}

void PdfAccessibilityTree::DoSetAccessibilityViewportInfo(
    const chrome_pdf::AccessibilityViewportInfo& viewport_info) {
  zoom_ = viewport_info.zoom;
  scale_ = viewport_info.scale;
  CHECK_GT(zoom_, 0);
  CHECK_GT(scale_, 0);
  scroll_ = gfx::PointF(viewport_info.scroll).OffsetFromOrigin();
  offset_ = gfx::PointF(viewport_info.offset).OffsetFromOrigin();

  selection_start_page_index_ = viewport_info.selection_start_page_index;
  selection_start_char_index_ = viewport_info.selection_start_char_index;
  selection_end_page_index_ = viewport_info.selection_end_page_index;
  selection_end_char_index_ = viewport_info.selection_end_char_index;

  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (render_accessibility && tree_.size() > 1) {
    ui::AXNode* root = tree_.root();
    ui::AXNodeData root_data = root->data();
    root_data.relative_bounds.transform = MakeTransformFromViewInfo();
    root->SetData(root_data);
    UpdateAXTreeDataFromSelection();
    render_accessibility->OnPluginRootNodeUpdated();
  }
}

void PdfAccessibilityTree::SetAccessibilityDocInfo(
    chrome_pdf::AccessibilityDocInfo doc_info) {
  // This call may trigger layout, and ultimately self-deletion; see
  // crbug.com/1274376 for details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfAccessibilityTree::DoSetAccessibilityDocInfo,
                     GetWeakPtr(), std::move(doc_info)));
}

void PdfAccessibilityTree::DoSetAccessibilityDocInfo(
    const chrome_pdf::AccessibilityDocInfo& doc_info) {
  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (!render_accessibility)
    return;

  ClearAccessibilityNodes();
  page_count_ = doc_info.page_count;
  doc_node_ =
      CreateNode(ax::mojom::Role::kPdfRoot, ax::mojom::Restriction::kReadOnly,
                 render_accessibility);
  doc_node_->AddState(ax::mojom::State::kFocusable);
  doc_node_->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                l10n_util::GetPluralStringFUTF8(
                                    IDS_PDF_DOCUMENT_PAGE_COUNT, page_count_));

  // Because all of the coordinates are expressed relative to the
  // doc's coordinates, the origin of the doc must be (0, 0). Its
  // width and height will be updated as we add each page so that the
  // doc's bounding box surrounds all pages.
  doc_node_->relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // This notification node needs to be added as the first node in the PDF
  // accessibility tree so that the user will reach out to this node first when
  // navigating the PDF accessibility tree.
  if (features::IsPdfOcrEnabled()) {
    ocr_status_node_ = CreateStatusNode(render_accessibility, doc_node_.get());
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

void PdfAccessibilityTree::SetAccessibilityPageInfo(
    chrome_pdf::AccessibilityPageInfo page_info,
    std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs,
    std::vector<chrome_pdf::AccessibilityCharInfo> chars,
    chrome_pdf::AccessibilityPageObjects page_objects) {
  // This call may trigger layout, and ultimately self-deletion; see
  // crbug.com/1274376 for details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfAccessibilityTree::DoSetAccessibilityPageInfo,
                     GetWeakPtr(), std::move(page_info), std::move(text_runs),
                     std::move(chars), std::move(page_objects)));
}

void PdfAccessibilityTree::DoSetAccessibilityPageInfo(
    const chrome_pdf::AccessibilityPageInfo& page_info,
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects) {
  // Outdated calls are ignored.
  uint32_t page_index = page_info.page_index;
  if (page_index != next_page_index_)
    return;

  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (!render_accessibility)
    return;

  // If unsanitized data is found, don't trust it and stop creation of the
  // accessibility tree.
  if (!invalid_plugin_message_received_) {
    invalid_plugin_message_received_ =
        !IsDataFromPluginValid(text_runs, chars, page_objects);
  }
  if (invalid_plugin_message_received_)
    return;

  CHECK_LT(page_index, page_count_);
  ++next_page_index_;

  AddPageContent(page_info, page_index, text_runs, chars, page_objects);

  did_get_a_text_run_ |= !text_runs.empty();
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsPdfOcrEnabled() && !did_get_a_text_run_) {
    if (ocr_service_) {
      // Notify users via the status node that PDF OCR is about to run since
      // the AXMode was set for PDF OCR.
      SetStatusMessage(IDS_PDF_OCR_IN_PROGRESS);
    } else {
      // Notify users of the PDF OCR feature when they encounter an inaccessible
      // PDF before turning on the feature via the status node.
      // TODO(crbug.com/1393069): Need to send out this notification only a few
      // times in case that the user may not want to use the PDF OCR feature
      // and to be notified of this feature anymore.
      SetStatusMessage(IDS_PDF_OCR_FEATURE_ALERT);
    }
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  if (page_index == page_count_ - 1)
    UnserializeNodes();
}

void PdfAccessibilityTree::AddPageContent(
    const chrome_pdf::AccessibilityPageInfo& page_info,
    uint32_t page_index,
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects) {
  CHECK(doc_node_);
  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  CHECK(render_accessibility);
  PdfAccessibilityTreeBuilder tree_builder(
      GetWeakPtr(), text_runs, chars, page_objects, page_info, page_index,
      doc_node_.get(), render_accessibility, &nodes_,
      &node_id_to_page_char_index_, &node_id_to_annotation_info_
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      ,
      ocr_service_.get(), did_get_a_text_run_
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  );
  tree_builder.BuildPageTree();
}

void PdfAccessibilityTree::UnserializeNodes() {
  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (!render_accessibility)
    return;

  doc_node_->relative_bounds.transform = MakeTransformFromViewInfo();

  ui::AXTreeUpdate update;
  // We need to set the `AXTreeID` both in the `AXTreeUpdate` and the
  // `AXTreeData` member because the constructor of `AXTree` might expect the
  // tree to be constructed with a valid tree ID.
  update.has_tree_data = true;
  update.tree_data.tree_id = render_accessibility->GetTreeIDForPluginHost();
  tree_data_.tree_id = render_accessibility->GetTreeIDForPluginHost();
  tree_data_.focus_id = doc_node_->id;
  update.root_id = doc_node_->id;
  update.nodes.push_back(*doc_node_);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsPdfOcrEnabled()) {
    update.nodes.push_back(*ocr_status_node_);
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  for (const auto& node : nodes_)
    update.nodes.push_back(std::move(*node));

  if (!tree_.Unserialize(update))
    LOG(FATAL) << tree_.error();

  UpdateAXTreeDataFromSelection();
  render_accessibility->SetPluginTreeSource(this);
  nodes_.clear();

  base::UmaHistogramBoolean("Accessibility.PDF.HasAccessibleText",
                            did_get_a_text_run_);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (!did_get_a_text_run_) {
    // TODO(crbug.com/1443345): Add a browser test to validate this UMA metric.
    base::UmaHistogramBoolean(
        "Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened",
        ocr_service_ != nullptr);
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void PdfAccessibilityTree::SetOcrCompleteStatus() {
  VLOG(2) << "Performing OCR on PDF is complete.";
  SetStatusMessage(IDS_PDF_OCR_COMPLETED);

  if (!nodes_.empty()) {
    // `nodes_` is not empty yet as `UnserializeNodes()` hasn't been called. In
    // this case, `ocr_status_node_` will be unserialized along with `nodes_`
    // when `UnserializeNodes()` gets called later.
    return;
  }

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  update.nodes.push_back(*ocr_status_node_);

  if (!tree_.Unserialize(update)) {
    LOG(FATAL) << tree_.error();
  }
}

void PdfAccessibilityTree::SetStatusMessage(int message_id) {
  DCHECK(ocr_status_node_);
  const std::string message = l10n_util::GetStringUTF8(message_id);
  VLOG(2) << "Setting the status node with message: " << message;
  ocr_status_node_->SetName(message);
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void PdfAccessibilityTree::UpdateAXTreeDataFromSelection() {
  tree_data_.sel_is_backward = false;
  if (selection_start_page_index_ > selection_end_page_index_) {
    tree_data_.sel_is_backward = true;
  } else if (selection_start_page_index_ == selection_end_page_index_ &&
             selection_start_char_index_ > selection_end_char_index_) {
    tree_data_.sel_is_backward = true;
  }

  FindNodeOffset(selection_start_page_index_, selection_start_char_index_,
                 &tree_data_.sel_anchor_object_id,
                 &tree_data_.sel_anchor_offset);
  FindNodeOffset(selection_end_page_index_, selection_end_char_index_,
                 &tree_data_.sel_focus_object_id, &tree_data_.sel_focus_offset);
}

void PdfAccessibilityTree::FindNodeOffset(uint32_t page_index,
                                          uint32_t page_char_index,
                                          int32_t* out_node_id,
                                          int32_t* out_node_char_index) const {
  *out_node_id = -1;
  *out_node_char_index = 0;
  ui::AXNode* root = tree_.root();
  if (page_index >= root->children().size())
    return;
  ui::AXNode* page = root->children()[page_index];

  // Iterate over all paragraphs within this given page, and static text nodes
  // within each paragraph.
  for (ui::AXNode* para : page->children()) {
    for (ui::AXNode* child_node : para->children()) {
      ui::AXNode* static_text = GetStaticTextNodeFromNode(child_node);
      if (!static_text)
        continue;
      // Look up the page-relative character index for static nodes from a map
      // we built while the document was initially built.
      auto iter = node_id_to_page_char_index_.find(static_text->id());
      uint32_t char_index = iter->second.char_index;
      uint32_t len = static_text->data()
                         .GetStringAttribute(ax::mojom::StringAttribute::kName)
                         .size();

      // If the character index we're looking for falls within the range
      // of this node, return the node ID and index within this node's text.
      if (page_char_index <= char_index + len) {
        *out_node_id = static_text->id();
        *out_node_char_index = page_char_index - char_index;
        return;
      }
    }
  }
}

bool PdfAccessibilityTree::FindCharacterOffset(
    const ui::AXNode& node,
    uint32_t char_offset_in_node,
    chrome_pdf::PageCharacterIndex& page_char_index) const {
  auto iter = node_id_to_page_char_index_.find(GetId(&node));
  if (iter == node_id_to_page_char_index_.end())
    return false;
  page_char_index.char_index = iter->second.char_index + char_offset_in_node;
  page_char_index.page_index = iter->second.page_index;
  return true;
}

void PdfAccessibilityTree::ClearAccessibilityNodes() {
  next_page_index_ = 0;
  doc_node_.reset();
  nodes_.clear();
  node_id_to_page_char_index_.clear();
  node_id_to_annotation_info_.clear();
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsPdfOcrEnabled()) {
    ocr_status_node_.reset();
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

content::RenderAccessibility* PdfAccessibilityTree::GetRenderAccessibility() {
  return render_frame() ? render_frame()->GetRenderAccessibility() : nullptr;
}

content::RenderAccessibility*
PdfAccessibilityTree::GetRenderAccessibilityIfEnabled() {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (!render_accessibility)
    return nullptr;

  // If RenderAccessibility is unable to generate valid positive IDs,
  // we shouldn't use it. This can happen if Blink accessibility is disabled
  // after we started generating the accessible PDF.
  base::WeakPtr<PdfAccessibilityTree> weak_this = GetWeakPtr();
  if (!render_accessibility->HasActiveDocument()) {
    return nullptr;
  }

  DCHECK(weak_this);

  return render_accessibility;
}

std::unique_ptr<gfx::Transform>
PdfAccessibilityTree::MakeTransformFromViewInfo() const {
  double applicable_scale_factor = scale_;
  auto transform = std::make_unique<gfx::Transform>();
  // `scroll_` represents the offset from which PDF content starts. It is the
  // height of the PDF toolbar and the width of sidenav in pixels if it is open.
  // Sizes of PDF toolbar and sidenav do not change with zoom.
  transform->Scale(applicable_scale_factor, applicable_scale_factor);
  transform->Translate(-scroll_);
  transform->Scale(zoom_, zoom_);
  transform->Translate(offset_);
  return transform;
}

PdfAccessibilityTree::AnnotationInfo::AnnotationInfo(uint32_t page_index,
                                                     uint32_t annotation_index)
    : page_index(page_index), annotation_index(annotation_index) {}

PdfAccessibilityTree::AnnotationInfo::AnnotationInfo(
    const AnnotationInfo& other) = default;

PdfAccessibilityTree::AnnotationInfo::~AnnotationInfo() = default;

//
// AXTreeSource implementation.
//

bool PdfAccessibilityTree::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = tree_data_.tree_id;
  tree_data->focus_id = tree_data_.focus_id;
  tree_data->sel_is_backward = tree_data_.sel_is_backward;
  tree_data->sel_anchor_object_id = tree_data_.sel_anchor_object_id;
  tree_data->sel_anchor_offset = tree_data_.sel_anchor_offset;
  tree_data->sel_focus_object_id = tree_data_.sel_focus_object_id;
  tree_data->sel_focus_offset = tree_data_.sel_focus_offset;
  return true;
}

ui::AXNode* PdfAccessibilityTree::GetRoot() const {
  return tree_.root();
}

ui::AXNode* PdfAccessibilityTree::GetFromId(int32_t id) const {
  return tree_.GetFromId(id);
}

int32_t PdfAccessibilityTree::GetId(const ui::AXNode* node) const {
  return node->id();
}

size_t PdfAccessibilityTree::GetChildCount(const ui::AXNode* node) const {
  return node->children().size();
}

const ui::AXNode* PdfAccessibilityTree::ChildAt(const ui::AXNode* node,
                                                size_t index) const {
  return node->children()[index];
}

ui::AXNode* PdfAccessibilityTree::GetParent(const ui::AXNode* node) const {
  return node->parent();
}

bool PdfAccessibilityTree::IsIgnored(const ui::AXNode* node) const {
  return node->IsIgnored();
}

bool PdfAccessibilityTree::IsEqual(const ui::AXNode* node1,
                                   const ui::AXNode* node2) const {
  return node1 == node2;
}

const ui::AXNode* PdfAccessibilityTree::GetNull() const {
  return nullptr;
}

void PdfAccessibilityTree::SerializeNode(const ui::AXNode* node,
                                         ui::AXNodeData* out_data) const {
  *out_data = node->data();
}

std::unique_ptr<ui::AXActionTarget> PdfAccessibilityTree::CreateActionTarget(
    const ui::AXNode& target_node) {
  return std::make_unique<PdfAXActionTarget>(target_node, this);
}

void PdfAccessibilityTree::AccessibilityModeChanged(const ui::AXMode& mode) {
  bool always_load_or_reload_accessibility = false;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (!mode.has_mode(ui::AXMode::kPDFOcr)) {
    if (ocr_service_) {
      VLOG(2) << "PDF OCR has been turned off. So, deleting OCR service.";
      ocr_service_.reset();
      // Need to perform LoadAccessibility() again to update PDF accessibility
      // tree without OCR results.
      always_load_or_reload_accessibility = true;
    }
    MaybeHandleAccessibilityChange(always_load_or_reload_accessibility);
    return;
  }

  if (ocr_service_) {
    return;
  }
  // TODO(crbug.com/1393069): Ensure that ui::AXMode::kPDFOcr is set in the
  // AXMode only when both the PDF OCR pref and screen reader are on.
  CreateOcrService();
  always_load_or_reload_accessibility = true;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  MaybeHandleAccessibilityChange(always_load_or_reload_accessibility);
}

void PdfAccessibilityTree::OnDestruct() {
  render_frame_ = nullptr;
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void PdfAccessibilityTree::OnOcrDataReceived(
    const ui::AXNodeID& image_node_id,
    const chrome_pdf::AccessibilityImageInfo& image,
    const ui::AXNodeID& parent_node_id,
    const ui::AXTreeUpdate& tree_update) {
  // TODO(accessibility): Following the convensions in this file, this method
  // manipulates the collection of `ui::AXNodeData` stored in the `nodes_` field
  // and then updates the `tree_` using the unserialize mechanism. It would be
  // more convenient and less complex if an `ui::AXTree` was never constructed
  // and if the `ui::AXTreeSource` was able to use the collection of `nodes_`
  // directly.

  // Check if `ocr_service_` is still available. If not, it means PDF OCR has
  // been turned off, so just return here to ignore OCR results.
  if (!ocr_service_) {
    return;
  }

  // TODO(crbug.com/1393069): Investigate more to understand cases in which
  // OCR gave no results and update the status node with a relevant message.
  if (ocr_service_->IsQueueEmpty()) {
    SetOcrCompleteStatus();
  }

  if (tree_update.nodes.empty()) {
    VLOG(1) << "Empty OCR data received.";
    return;
  }

  VLOG(1) << "OCR data received with a child tree update's root id: "
          << tree_update.root_id;
  // `tree_update` encodes a subtree that is going to be added to the PDF
  // accessibility tree directly. Thus, `tree_update.root_id` isn't the root of
  // the PDF accessibility tree, but the root of the subtree being added.
  const ui::AXNodeID& page_node_id = tree_update.root_id;
  DCHECK_NE(page_node_id, ui::kInvalidAXNodeID);

  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (!render_accessibility) {
    return;
  }

  const gfx::RectF& image_bounds = image.bounds;
  DCHECK_NE(image_node_id, ui::kInvalidAXNodeID);
  DCHECK_NE(parent_node_id, ui::kInvalidAXNodeID);
  DCHECK(!image_bounds.IsEmpty());
  DCHECK(doc_node_);

  // `nodes_` will be empty once they are unserialized to `tree_`.
  bool did_unserialize_once = nodes_.empty();
#if DCHECK_IS_ON()
  if (!did_unserialize_once) {
    DCHECK(ranges::find_if(
               nodes_,
               [&image_node_id](const std::unique_ptr<ui::AXNodeData>& node) {
                 return node->id == image_node_id;
               }) != ranges::end(nodes_));
  }
#endif

  // Create a Transform to position OCR results on PDF. Without this transform,
  // nodes created from OCR results will have misaligned bounding boxes. This
  // transform will be applied to all nodes from OCR results below.
  gfx::Transform transform = MakeTransformForImage(image);

  // Copy nodes from `AXTreeUpdate` from OCR results and update their relative
  // bounds. PDF accessibility tree assumes that all nodes have bounds relative
  // to the root node.
  // TODO(crbug.com/1278249): add an attribute to indicate that these nodes
  // are auto-generated and have a way to notify the user of that.
  for (const auto& node_from_ocr : tree_update.nodes) {
    std::unique_ptr<ui::AXNodeData> new_node =
        std::make_unique<ui::AXNodeData>(node_from_ocr);
    if (new_node->id == page_node_id) {
      // This page node will be replaced with the image node, so it needs to
      // have the image node's bounds.
      new_node->relative_bounds.bounds = image_bounds;
    } else {
      new_node->relative_bounds.bounds =
          transform.MapRect(new_node->relative_bounds.bounds);
      // Make all the other nodes relative to the page node.
      new_node->relative_bounds.bounds.Offset(image_bounds.x(),
                                              image_bounds.y());
    }
    // Make all nodes relative to the root node.
    new_node->relative_bounds.offset_container_id = doc_node_->id;
    // Move to `nodes_` for later unserialization in `UnserializeNodes()`.
    nodes_.push_back(std::move(new_node));
  }

  if (!did_unserialize_once) {
    // `nodes_` have not been unserialized yet, so update `nodes_` directly and
    // return. `nodes_` will be unserialized in `UnserializeNodes()` later.
    int num_erased = base::EraseIf(
        nodes_, [&image_node_id](const std::unique_ptr<ui::AXNodeData>& node) {
          return node->id == image_node_id;
        });
    CHECK_EQ(num_erased, 1);
    const auto parent_node_iter = ranges::find_if(
        nodes_, [&parent_node_id](const std::unique_ptr<ui::AXNodeData>& node) {
          return node->id == parent_node_id;
        });
    CHECK(parent_node_iter != ranges::end(nodes_));
    num_erased = base::Erase((*parent_node_iter)->child_ids, image_node_id);
    CHECK_EQ(num_erased, 1);
    (*parent_node_iter)->child_ids.push_back(page_node_id);
    return;
  }

  // Create a new AXTreeUpdate only after `tree_` being unserialized in
  // UnserializeNodes(). Otherwise, it may try updating a AXNodeData that is
  // not existed in `tree_` yet, which will lead to an error.
  ui::AXNode* parent_node_ptr = tree_.GetFromId(parent_node_id);
  ui::AXNodeData parent_node(parent_node_ptr->data());

  int num_erased = base::Erase(parent_node.child_ids, image_node_id);
  CHECK_EQ(num_erased, 1);
  parent_node.child_ids.push_back(page_node_id);

  ui::AXTreeUpdate update;
  update.node_id_to_clear = image_node_id;
  update.root_id = doc_node_->id;
  update.nodes.push_back(std::move(parent_node));
  for (const auto& node : nodes_) {
    update.nodes.push_back(std::move(*node));
  }

  if (!tree_.Unserialize(update)) {
    LOG(FATAL) << tree_.error();
  }

  render_accessibility->SetPluginTreeSource(this);
  nodes_.clear();
}

void PdfAccessibilityTree::CreateOcrService() {
  VLOG(2) << "Creating OCR service.";
  ocr_service_ = std::make_unique<PdfOcrService>(
      *render_frame_,
      base::BindRepeating(&PdfAccessibilityTree::OnOcrDataReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

bool PdfAccessibilityTree::ShowContextMenu() {
  content::RenderAccessibility* render_accessibility =
      GetRenderAccessibilityIfEnabled();
  if (!render_accessibility)
    return false;

  render_accessibility->ShowPluginContextMenu();
  return true;
}

void PdfAccessibilityTree::HandleAction(
    const chrome_pdf::AccessibilityActionData& action_data) {
  action_handler_->HandleAccessibilityAction(action_data);
}

absl::optional<PdfAccessibilityTree::AnnotationInfo>
PdfAccessibilityTree::GetPdfAnnotationInfoFromAXNode(int32_t ax_node_id) const {
  auto iter = node_id_to_annotation_info_.find(ax_node_id);
  if (iter == node_id_to_annotation_info_.end())
    return absl::nullopt;

  return AnnotationInfo(iter->second.page_index, iter->second.annotation_index);
}

void PdfAccessibilityTree::MaybeHandleAccessibilityChange(
    bool always_load_or_reload_accessibility) {
  if (GetRenderAccessibility()) {
    if (always_load_or_reload_accessibility) {
      action_handler_->LoadOrReloadAccessibility();
    } else {
      action_handler_->EnableAccessibility();
    }
  }
}

}  // namespace pdf
