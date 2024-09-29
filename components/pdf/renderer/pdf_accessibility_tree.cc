// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/pdf/renderer/pdf_accessibility_tree_builder.h"
#include "components/pdf/renderer/pdf_ax_action_target.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "pdf/pdf_accessibility_action_handler.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "base/containers/contains.h"
#include "services/screen_ai/public/cpp/metrics.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace pdf {

namespace ranges = base::ranges;

namespace {

// Delay before loading all the PDF content into the accessibility tree and
// resetting the banner and status nodes in an accessibility tree.
constexpr base::TimeDelta kDelayBeforeResettingStatusNode = base::Seconds(1);

enum class AttributeUpdateType {
  kRemove = 0,
  kAdd,
};

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

template <typename T>
bool CompareTextRuns(const T& a, const T& b) {
  return a.text_run_index < b.text_run_index;
}

template <typename T>
bool CompareTextRunsWithRange(const T& a, const T& b) {
  return a.text_range.index < b.text_range.index;
}

std::unique_ptr<ui::AXNodeData> CreateNode(ax::mojom::Role role,
                                           ax::mojom::Restriction restriction,
                                           ui::AXNodeID id) {
  auto node = std::make_unique<ui::AXNodeData>();
  node->id = id;
  node->role = role;
  node->SetRestriction(restriction);

  return node;
}

void UpdateStatusNodeLiveRegionAttributes(ui::AXNodeData* node,
                                          AttributeUpdateType update_type) {
  CHECK(node);
  switch (update_type) {
    case AttributeUpdateType::kAdd:
      // Encode ARIA live region attributes including aria-atomic, aria-status,
      // and aria-relevant to define aria-live="polite" for this status node.
      node->AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
      node->AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                               "polite");
      node->AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                               "additions text");
      // The status node is the root of live region. Use `kContainerLive*`
      // attributes to define this node as the root of the live region.
      node->AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                             true);
      node->AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus,
                               "polite");
      node->AddStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveRelevant, "additions text");
      break;
    case AttributeUpdateType::kRemove:
      node->RemoveBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic);
      node->RemoveStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
      node->RemoveStringAttribute(ax::mojom::StringAttribute::kLiveRelevant);
      node->RemoveBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveAtomic);
      node->RemoveStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveStatus);
      node->RemoveStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveRelevant);
      break;
  }
}

std::unique_ptr<ui::AXNodeData> CreateStatusNodeStaticText(
    ui::AXNodeID id,
    ui::AXNodeData* parent_node) {
  // Creates a static text node for the status node to make it look like a
  // rendered text.
  std::unique_ptr<ui::AXNodeData> node = CreateNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly, id);
  node->relative_bounds = parent_node->relative_bounds;
  node->AddStringAttribute(ax::mojom::StringAttribute::kName, std::string());

  // The static text node will be added as the first node to its parent node as
  // the parent node will contain only this static text node.
  CHECK(parent_node->child_ids.empty());
  parent_node->child_ids.push_back(node->id);
  VLOG(2) << "Creating a static text for OCR status node.";
  return node;
}

std::unique_ptr<ui::AXNodeData> CreateStatusNode(ui::AXNodeID id,
                                                 ui::AXNodeData* parent_node,
                                                 bool currently_in_foreground) {
  // Create a status node that conveys a notification message and place the
  // message inside an appropriate ARIA landmark for easy navigation.
  std::unique_ptr<ui::AXNodeData> node = CreateNode(
      ax::mojom::Role::kStatus, ax::mojom::Restriction::kReadOnly, id);
  node->relative_bounds = parent_node->relative_bounds;
  if (currently_in_foreground) {
    UpdateStatusNodeLiveRegionAttributes(node.get(), AttributeUpdateType::kAdd);
  }

  // The status node will be added as the first node to its parent node as the
  // parent node will contain only this status node.
  CHECK(parent_node->child_ids.empty());
  parent_node->child_ids.push_back(node->id);
  VLOG(2) << "Creating an OCR status node.";
  return node;
}

// TODO(crbug.com/326131114): May need to give it a proper name or title.
// Revisit this banner node to understand why it is here besides navigation.
std::unique_ptr<ui::AXNodeData> CreateBannerNode(ui::AXNodeID id,
                                                 ui::AXNodeData* root_node) {
  // Create a banner node with an appropriate ARIA landmark for easy navigation.
  // This banner node will contain a status node later.
  std::unique_ptr<ui::AXNodeData> banner_node = CreateNode(
      ax::mojom::Role::kBanner, ax::mojom::Restriction::kReadOnly, id);
  // Set the origin of this node to be offscreen with an 1x1 rectangle as
  // both this wrapper and a status node don't have a visual element. The origin
  // of the doc is (0, 0), so setting (-1, -1) will make this node offscreen.
  banner_node->relative_bounds.bounds = gfx::RectF(-1, -1, 1, 1);
  banner_node->relative_bounds.offset_container_id = root_node->id;
  // As we create this status node's wrapper right after the PDF root node,
  // this node will be added as the first node to the PDF accessibility tree.
  CHECK(root_node->child_ids.empty());
  root_node->child_ids.push_back(banner_node->id);
  return banner_node;
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
gfx::Transform MakeTransformForImage(const gfx::RectF image_screen_size,
                                     const gfx::SizeF image_pixel_size,
                                     const int32_t orientation) {
  // Nodes created with OCR results from the image will be misaligned on screen
  // if `image_screen_size` is different from `image_pixel_size`. To address
  // this misalignment issue, an additional transform needs to be created.
  CHECK(!image_pixel_size.IsEmpty());

  gfx::Transform transform;
  // Note that the `Translate`, `Scale`, and `Rotate` steps are combined into
  // one transform matrix and applied together. The `Translate` step sets the
  // 3rd row of the 3x3 transform matrix, and the other two set the first two
  // rows. Applying these steps one by one separately does not result in the
  // same transform as the combined one.
  switch (orientation) {
    case 0:
      break;
    case 1:
      transform.Translate(image_screen_size.height(), 0);
      break;
    case 2:
      transform.Translate(image_screen_size.width(),
                          image_screen_size.height());
      break;
    case 3:
      transform.Translate(0, image_screen_size.width());
      break;
    default:
      NOTREACHED();
  }
  transform.Scale(image_screen_size.width() / image_pixel_size.width(),
                  image_screen_size.height() / image_pixel_size.height());
  transform.Rotate(orientation * 90);

  return transform;
}

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

// When PDF Searchify is enabled, inaccessible PDFs are made accessible in
// PDFium by sending their images to the OCR service and adding the recognized
// text to the PDF. Hence they don't need extra work here.
bool PdfOcrInRenderer() {
  return features::IsPdfOcrEnabled() &&
         !base::FeatureList::IsEnabled(chrome_pdf::features::kPdfSearchify);
}
}  // namespace

PdfAccessibilityTree::PdfAccessibilityTree(
    content::RenderFrame* render_frame,
    chrome_pdf::PdfAccessibilityActionHandler* action_handler,
    chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
    blink::WebPluginContainer* plugin_container,
    bool print_preview)
    : content::RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      action_handler_(action_handler),
      image_fetcher_(image_fetcher),
      plugin_container_(plugin_container),
      print_preview_(print_preview) {
  DCHECK(render_frame);
  DCHECK(action_handler_);
  DCHECK(image_fetcher_);
  MaybeHandleAccessibilityChange(/*always_load_or_reload_accessibility=*/false);
}

PdfAccessibilityTree::~PdfAccessibilityTree() {
  UpdateDependentObjects(/*set_this=*/false);
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
  orientation_ = viewport_info.orientation;
  selection_start_page_index_ = viewport_info.selection_start_page_index;
  selection_start_char_index_ = viewport_info.selection_start_char_index;
  selection_end_page_index_ = viewport_info.selection_end_page_index;
  selection_end_char_index_ = viewport_info.selection_end_char_index;

  auto obj = GetPluginContainerAXObject();
  if (obj && tree_.size() > 1) {
    ui::AXNode* root = tree_.root();
    ui::AXNodeData root_data = root->data();
    root_data.relative_bounds.transform = MakeTransformFromViewInfo();
    root->SetData(root_data);
    UpdateAXTreeDataFromSelection();
    MarkPluginContainerDirty();
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
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  ClearAccessibilityNodes();
  page_count_ = doc_info.page_count;

  doc_node_ =
      CreateNode(ax::mojom::Role::kPdfRoot, ax::mojom::Restriction::kReadOnly,
                 obj->GenerateAXID());
  doc_node_->AddState(ax::mojom::State::kFocusable);
  doc_node_->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                l10n_util::GetPluralStringFUTF8(
                                    IDS_PDF_DOCUMENT_PAGE_COUNT, page_count_));

  // Because all of the coordinates are expressed relative to the
  // doc's coordinates, the origin of the doc must be (0, 0). Its
  // width and height will be updated as we add each page so that the
  // doc's bounding box surrounds all pages.
  doc_node_->relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);

  // This notification node needs to be added as the first node in the PDF
  // accessibility tree so that the user will reach out to this node first when
  // navigating the PDF accessibility tree.
  banner_node_ = CreateBannerNode(obj->GenerateAXID(), doc_node_.get());
  status_node_ = CreateStatusNode(obj->GenerateAXID(), banner_node_.get(),
                                  currently_in_foreground_);
  status_node_text_ =
      CreateStatusNodeStaticText(obj->GenerateAXID(), status_node_.get());

  SetStatusMessage(IDS_PDF_LOADING_TO_A11Y_TREE);

  // Create a PDF accessibility tree with the status node first to notify users
  // that PDF content is being loaded.
  ui::AXTreeUpdate update;
  // We need to set the `AXTreeID` both in the `AXTreeUpdate` and the
  // `AXTreeData` member because the constructor of `AXTree` might expect the
  // tree to be constructed with a valid tree ID.
  update.has_tree_data = true;
  const auto& tree_id = render_frame_->GetWebFrame()->GetAXTreeID();
  update.tree_data.tree_id = tree_id;
  tree_data_.tree_id = tree_id;
  tree_data_.focus_id = doc_node_->id;
  update.root_id = doc_node_->id;
  update.nodes = {*doc_node_, *banner_node_, *status_node_, *status_node_text_};
  if (!tree_.Unserialize(update)) {
    LOG(FATAL) << tree_.error();
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (ocr_helper_) {
    ocr_helper_->Reset(doc_node_->id, page_count_);
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  MarkPluginContainerDirty();
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

  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  // If unsanitized data is found, don't trust it and stop creation of the
  // accessibility tree. Now that we already created the initial tree with the
  // root node and the status node, destroy the existing tree as well.
  if (!invalid_plugin_message_received_) {
    invalid_plugin_message_received_ =
        !IsDataFromPluginValid(text_runs, chars, page_objects);
  }
  if (invalid_plugin_message_received_) {
    if (tree_.root()) {
      tree_.Destroy();
      banner_node_.reset();
      status_node_.reset();
      status_node_text_.reset();
    }
    return;
  }

  CHECK_LT(page_index, page_count_);
  ++next_page_index_;
  // Update `did_get_a_text_run_` before calling `AddPageContent()` as this
  // variable will be used inside of `AddPageContent()`.
  did_get_a_text_run_ |= !text_runs.empty();

  AddPageContent(page_info, page_index, text_runs, chars, page_objects);

  bool has_image = !page_objects.images.empty();
  did_have_an_image_ |= has_image;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // TODO(crbug.com/40267312): Use a more explicit flag indicating whether any
  // image was sent to the OCR model in `AddRemainingAnnotations()`.
  if (PdfOcrInRenderer() && !did_get_a_text_run_ && has_image) {
    if (ocr_helper_) {
      // Notify users via the status node that PDF OCR is about to run since
      // the AXMode was set for PDF OCR.
      SetStatusMessage(IDS_PDF_OCR_IN_PROGRESS);
    } else {
      if (page_index == page_count_ - 1) {
        // Set the status node for PDF OCR feature notification after adding
        // the last page's content.
        SetStatusMessage(IDS_PDF_OCR_FEATURE_ALERT);
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  if (page_index == page_count_ - 1) {
    if (!PdfOcrInRenderer() || did_get_a_text_run_ || !did_have_an_image_) {
      // In this case, PDF OCR doesn't run. Thus, set the status node to notify
      // users that the PDF content has been loaded into an accessibility tree.
      SetStatusMessage(IDS_PDF_LOADED_TO_A11Y_TREE);

      UnserializeNodes();
      // Reset the status node's attributes after a delay. This delay allows
      // screen reader to deliver the user the notification message set above.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PdfAccessibilityTree::ResetStatusNodeAttributes,
                         GetWeakPtr()),
          kDelayBeforeResettingStatusNode);
    } else {
      UnserializeNodes();
    }
  }
}

void PdfAccessibilityTree::AddPageContent(
    const chrome_pdf::AccessibilityPageInfo& page_info,
    uint32_t page_index,
    const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
    const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
    const chrome_pdf::AccessibilityPageObjects& page_objects) {
  CHECK(doc_node_);
  auto obj = GetPluginContainerAXObject();
  CHECK(obj);
  PdfAccessibilityTreeBuilder tree_builder(
      GetWeakPtr(), text_runs, chars, page_objects, page_info, page_index,
      doc_node_.get(), &(*obj), &nodes_, &node_id_to_page_char_index_,
      &node_id_to_annotation_info_
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      ,
      ocr_helper_.get(), did_get_a_text_run_
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  );
  tree_builder.BuildPageTree();
}

void PdfAccessibilityTree::UnserializeNodes() {
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  doc_node_->relative_bounds.transform = MakeTransformFromViewInfo();

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  update.nodes.push_back(*doc_node_);
  update.nodes.push_back(*status_node_);
  update.nodes.push_back(*status_node_text_);
  for (const auto& node : nodes_) {
    obj->MarkPluginDescendantDirty(node->id);
    update.nodes.push_back(std::move(*node));
  }

  if (!tree_.Unserialize(update))
    LOG(FATAL) << tree_.error();

  UpdateAXTreeDataFromSelection();

  MarkPluginContainerDirty();

  nodes_.clear();

  if (!sent_metrics_once_) {
    // If the user turns on PDF OCR after opening a PDF, its PDF a11y tree gets
    // created again. `sent_metrics_once_` helps to determine whether
    // it's first time to create a PDF a11y tree. When a PDF is opened, the UMA
    // metrics need be recorded once.
    sent_metrics_once_ = true;

    base::UmaHistogramBoolean("Accessibility.PDF.HasAccessibleText",
                              did_get_a_text_run_);

    // TODO(accessibility): remove this dependency.
    content::RenderAccessibility* render_accessibility =
        render_frame() ? render_frame()->GetRenderAccessibility() : nullptr;
    CHECK(render_accessibility);

    if (!did_get_a_text_run_) {
      base::UmaHistogramCounts1000(
          "Accessibility.PdfOcr.InaccessiblePdfPageCount", page_count_);
      render_accessibility->RecordInaccessiblePdfUkm();
    }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // TODO(crbug.com/40070182): Update this and other cases with a
    // `IsAccessiblePDF` function.
    if (PdfOcrInRenderer() && !did_get_a_text_run_) {
      base::UmaHistogramBoolean(
          "Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened",
          ocr_helper_ != nullptr);
    }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  }
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void PdfAccessibilityTree::SetOcrCompleteStatus() {
  VLOG(2) << "Performing OCR on PDF is complete.";

  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  SetStatusMessage(was_text_converted_from_image_ ? IDS_PDF_OCR_COMPLETED
                                                  : IDS_PDF_OCR_NO_RESULT);

  if (!nodes_.empty()) {
    // `nodes_` is not empty yet as `UnserializeNodes()` hasn't been called. In
    // this case, `status_node_` will be unserialized along with `nodes_` when
    // `UnserializeNodes()` gets called later.
    return;
  }

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  update.nodes.push_back(*status_node_);
  update.nodes.push_back(*status_node_text_);

  if (!tree_.Unserialize(update)) {
    LOG(FATAL) << tree_.error();
  }
  MarkPluginContainerDirty();

#if BUILDFLAG(IS_CHROMEOS)
  // `FireLayoutComplete()` will be captured by Select-to-Speak on ChromeOS for
  // the "Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened" metric.
  // TODO(crbug/289010799): Remove `FireLayoutComplete()` when the
  // Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened histogram expires.
  CHECK(render_frame());
  content::RenderAccessibility* render_accessibility =
      render_frame()->GetRenderAccessibility();
  CHECK(render_accessibility);
  render_accessibility->FireLayoutComplete();
#endif  // BUILDFLAG(IS_CHROMEOS)
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void PdfAccessibilityTree::SetStatusMessage(int message_id) {
  CHECK(status_node_);
  CHECK(status_node_text_);
  const std::string message = l10n_util::GetStringUTF8(message_id);
  VLOG(2) << "Setting the status node with message: " << message;
  status_node_->SetNameChecked(message);
  status_node_text_->SetNameChecked(message);

  auto obj = GetPluginContainerAXObject();
  CHECK(obj);
  obj->MarkPluginDescendantDirty(banner_node_->id);
}

void PdfAccessibilityTree::ResetStatusNodeAttributes() {
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  CHECK(status_node_);
  CHECK(status_node_text_);
  // Clear out its live region and name attributes as it is no longer necessary
  // to keep the status node in this case. The node may not have live region
  // attributes. However, it is okay to try removing them from the node as
  // removing will be performed only when the node has those attributes.
  UpdateStatusNodeLiveRegionAttributes(status_node_.get(),
                                       AttributeUpdateType::kRemove);
  status_node_->RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  status_node_text_->RemoveStringAttribute(ax::mojom::StringAttribute::kName);

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  // `status_node_` has been either cleared out or set with a new message, so
  // add it to `ui::AXTreeUpdate`.
  update.nodes.push_back(*status_node_);
  update.nodes.push_back(*status_node_text_);
  if (!tree_.Unserialize(update)) {
    LOG(FATAL) << tree_.error();
  }
  MarkPluginContainerDirty();
}

void PdfAccessibilityTree::UpdateAXTreeDataFromSelection() {
  // The tree should contain a node for each page and one additional node, a
  // status node (see UnserializeNodes()). If the tree is not yet fully
  // populated with these nodes, a selection should not be possible.
  if (page_count_ != tree_.root()->children().size() - 1) {
    return;
  }

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
  // Use page_index + 1 since the first node in the tree is the status node, not
  // an actual page node.
  if (page_index + 1 >= root->children().size()) {
    return;
  }
  ui::AXNode* page = root->children()[page_index + 1];

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
  status_node_.reset();
  banner_node_.reset();
  nodes_.clear();
  node_id_to_page_char_index_.clear();
  node_id_to_annotation_info_.clear();
}

std::optional<blink::WebAXObject>
PdfAccessibilityTree::GetPluginContainerAXObject() {
  // Might be nullptr within tests.
  if (!plugin_container_) {
    CHECK_IS_TEST();
    if (force_plugin_ax_object_for_testing_.IsDetached()) {
      return std::nullopt;
    }
    return blink::WebAXObject(force_plugin_ax_object_for_testing_);
  }

  const blink::WebAXObject& obj =
      blink::WebAXObject::FromWebNode(plugin_container_->GetElement());
  if (obj.IsDetached()) {
    return std::nullopt;
  }
  return obj;
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
  // This tree may not yet be fully constructed.
  if (!tree_.root()) {
    return false;
  }

  tree_data->tree_id = render_frame_->GetWebFrame()->GetAXTreeID();
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
    ui::AXNodeID id) {
  ui::AXNode* target_node = GetFromId(id);
  if (!target_node) {
    return std::make_unique<ui::NullAXActionTarget>();
  }
  return std::make_unique<PdfAXActionTarget>(*target_node, this);
}

void PdfAccessibilityTree::AccessibilityModeChanged(const ui::AXMode& mode) {
  if (mode.is_mode_off()) {
    UpdateDependentObjects(/*set_this=*/false);
    return;
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (!mode.has_mode(ui::AXMode::kPDFOcr)) {
    if (ocr_helper_) {
      VLOG(2) << "PDF OCR has been turned off. So, deleting OCR helper.";
      ocr_helper_.reset();
    }
    MaybeHandleAccessibilityChange(
        /*always_load_or_reload_accessibility=*/true);
    return;
  }

  if (ocr_helper_) {
    return;
  }
  CreateOcrHelper();
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  MaybeHandleAccessibilityChange(
      /*always_load_or_reload_accessibility=*/true);
}

void PdfAccessibilityTree::OnDestruct() {
  render_frame_ = nullptr;
}

void PdfAccessibilityTree::WasHidden() {
  currently_in_foreground_ = false;
}

void PdfAccessibilityTree::WasShown() {
  currently_in_foreground_ = true;
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void PdfAccessibilityTree::OnOcrDataReceived(
    std::vector<PdfOcrRequest> ocr_requests,
    std::vector<ui::AXTreeUpdate> tree_updates) {
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return;
  }

  // Check if `ocr_helper_` is still available. If not, it means PDF OCR has
  // been turned off, so just return here to ignore OCR results.
  if (!ocr_helper_) {
    return;
  }

  // `nodes_` will be empty once they are unserialized to `tree_`.
  bool unserialized_node_exist = !nodes_.empty();
  CHECK(doc_node_);
  CHECK_GT(ocr_requests.size(), 0u);
  CHECK_EQ(ocr_requests.size(), tree_updates.size());
  obj->MarkPluginDescendantDirty(doc_node_->id);
  for (uint32_t i = 0; i < ocr_requests.size(); ++i) {
    const PdfOcrRequest& ocr_request = ocr_requests[i];
    ui::AXTreeUpdate& tree_update = tree_updates[i];

    // TODO(accessibility): Following the convensions in this file, this method
    // manipulates the collection of `ui::AXNodeData` stored in the `nodes_`
    // field and then updates the `tree_` using the unserialize mechanism. It
    // would be more convenient and less complex if an `ui::AXTree` was never
    // constructed and if the `ui::AXTreeSource` was able to use the collection
    // of `nodes_` directly.

    if (tree_update.nodes.empty()) {
      VLOG(1) << "Empty OCR data received.";
      // This can happen if OCR returns an empty result, or the image draws
      // nothing. Need to keep iterating the rest of `tree_updates` as there
      // can be some updates after this empty update in `tree_updates`. If the
      // image doesn't have alt text, it needs to be labeled with the default
      // label, `IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION`, which is set to an
      // image without alt text on a PDF or webpage.
      if (unserialized_node_exist) {
        const auto image_node_iter = ranges::find_if(
            nodes_,
            [&ocr_request](const std::unique_ptr<ui::AXNodeData>& node) {
              return node->id == ocr_request.image_node_id;
            });
        CHECK(image_node_iter != std::ranges::end(nodes_));
        if (ocr_request.image.alt_text.empty()) {
          // TODO(crbug.com/289010799): Add a CHECK to ensure that the image
          // node was labeled with `IDS_PDF_OCR_IN_PROGRESS_AX_UNLABELED_IMAGE`
          // when it's sent to OCR.
          (*image_node_iter)
              ->SetNameChecked(l10n_util::GetStringUTF8(
                  IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
        }
      } else {
        // No pending updates contained in `nodes_`, so we call `Unserialize()`
        // directly.
        ui::AXNode* image_node = tree_.GetFromId(ocr_request.image_node_id);
        CHECK(image_node);
        ui::AXNodeData image_node_data = image_node->data();
        if (ocr_request.image.alt_text.empty()) {
          // TODO(crbug.com/289010799): Add a CHECK to ensure that the image
          // node was labeled with `IDS_PDF_OCR_IN_PROGRESS_AX_UNLABELED_IMAGE`
          // when it's sent to OCR.
          image_node_data.SetNameChecked(l10n_util::GetStringUTF8(
              IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
        }
        tree_update.root_id = doc_node_->id;
        tree_update.nodes.emplace_back(std::move(image_node_data));
        if (!tree_.Unserialize(tree_update)) {
          LOG(FATAL) << tree_.error();
        }
      }
      continue;
    }

    // Update the flag if OCR extracted text from any images. This flag will be
    // used to update the status node to notify users of it.
    was_text_converted_from_image_ = true;
    VLOG(1) << "OCR data received with a child tree update's root id: "
            << tree_update.root_id;
    // `tree_update` encodes a subtree that is going to be added to the PDF
    // accessibility tree directly. Thus, `tree_update.root_id` isn't the root
    // of the PDF accessibility tree, but the root of the subtree being added.
    const ui::AXNodeID& extracted_text_root_node_id = tree_update.root_id;
    CHECK_NE(extracted_text_root_node_id, ui::kInvalidAXNodeID);

    const gfx::RectF& image_bounds = ocr_request.image.bounds;
    CHECK_NE(ocr_request.image_node_id, ui::kInvalidAXNodeID);
    CHECK_NE(ocr_request.parent_node_id, ui::kInvalidAXNodeID);
    CHECK(!image_bounds.IsEmpty());

#if DCHECK_IS_ON()
    if (unserialized_node_exist) {
      DCHECK(ranges::find_if(
                 nodes_,
                 [&ocr_request](const std::unique_ptr<ui::AXNodeData>& node) {
                   return node->id == ocr_request.image_node_id;
                 }) != std::ranges::end(nodes_));
    }
#endif

    // Create a Transform to position OCR results on PDF. Without this
    // transform, nodes created from OCR results will have misaligned bounding
    // boxes. This transform will be applied to all nodes from OCR results
    // below.
    gfx::Transform transform = MakeTransformForImage(
        ocr_request.image.bounds, ocr_request.image_pixel_size, orientation_);

    // Update the relative bounds of all nodes in the tree update. The PDF
    // accessibility tree assumes that all nodes have bounds relative to the
    // root node.
    for (auto& node_from_ocr : tree_update.nodes) {
      if (node_from_ocr.id == extracted_text_root_node_id) {
        // This page node will replace the image node, so it needs to have the
        // image node's bounds.
        node_from_ocr.relative_bounds.bounds = image_bounds;
      } else {
        int original_width = node_from_ocr.relative_bounds.bounds.width();
        node_from_ocr.relative_bounds.bounds =
            transform.MapRect(node_from_ocr.relative_bounds.bounds);
        // Make all the other nodes relative to the page node.
        node_from_ocr.relative_bounds.bounds.Offset(image_bounds.x(),
                                                    image_bounds.y());

        // Character offsets are computed in pixel and based on the image size
        // that is sent to OCR. They should be scaled if the view size is
        // different.
        int new_width = (orientation_ == 0 || orientation_ == 2)
                            ? node_from_ocr.relative_bounds.bounds.width()
                            : node_from_ocr.relative_bounds.bounds.height();
        if (node_from_ocr.HasIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets) &&
            new_width != original_width) {
          std::vector<int32_t> character_offsets =
              node_from_ocr.GetIntListAttribute(
                  ax::mojom::IntListAttribute::kCharacterOffsets);

          float ratio = static_cast<float>(new_width) / original_width;
          base::ranges::for_each(character_offsets, [ratio](int32_t& offset) {
            offset = static_cast<int32_t>(offset * ratio);
          });
          node_from_ocr.AddIntListAttribute(
              ax::mojom::IntListAttribute::kCharacterOffsets,
              character_offsets);
        }
      }
      // Make all nodes relative to the root node.
      node_from_ocr.relative_bounds.offset_container_id = doc_node_->id;
    }
    screen_ai::RecordMostDetectedLanguageInOcrData(
        "Accessibility.PdfOcr.MostDetectedLanguageInOcrData2", tree_update);

    if (unserialized_node_exist) {
      // `nodes_` have not been unserialized yet, so update `nodes_` directly
      // and return. `nodes_` will be unserialized in `UnserializeNodes()`
      // later.
      ranges::transform(tree_update.nodes, std::back_inserter(nodes_),
                        [](const ui::AXNodeData& node) {
                          return std::make_unique<ui::AXNodeData>(node);
                        });
      int num_erased = std::erase_if(
          nodes_, [&ocr_request](const std::unique_ptr<ui::AXNodeData>& node) {
            return node->id == ocr_request.image_node_id;
          });
      CHECK_EQ(num_erased, 1);

      const auto parent_node_iter = ranges::find_if(
          nodes_, [&ocr_request](const std::unique_ptr<ui::AXNodeData>& node) {
            return node->id == ocr_request.parent_node_id;
          });
      CHECK(parent_node_iter != std::ranges::end(nodes_));
      num_erased =
          std::erase((*parent_node_iter)->child_ids, ocr_request.image_node_id);
      CHECK_EQ(num_erased, 1);
      (*parent_node_iter)->child_ids.push_back(extracted_text_root_node_id);
      // Because we now have OCR results, the parenting node can no longer be a
      // paragraph as OCR's tree contains its own paragraph. A generic
      // container is equivalent to a div.
      (*parent_node_iter)->role = ax::mojom::Role::kGenericContainer;
      // Need to keep iterating the rest of `tree_updates`.
      continue;
    }

    // Create a new `AXTreeUpdate` only after `tree_` has been unserialized in
    // `UnserializeNodes()`. Otherwise, it may try updating an `AXNodeData` that
    // does not exist in `tree_` yet, which will lead to an error.
    ui::AXNode* parent_node = tree_.GetFromId(ocr_request.parent_node_id);
    CHECK(parent_node);
    ui::AXNodeData parent_node_data = parent_node->data();
    int num_erased =
        std::erase(parent_node_data.child_ids, ocr_request.image_node_id);
    CHECK_EQ(num_erased, 1);
    parent_node_data.child_ids.push_back(extracted_text_root_node_id);
    // Because we now have OCR results, the parenting node can no longer be a
    // paragraph as OCR's tree contains its own paragraph. A generic container
    // is equivalent to a div.
    parent_node_data.role = ax::mojom::Role::kGenericContainer;
    tree_update.root_id = doc_node_->id;
    tree_update.nodes.insert(tree_update.nodes.begin(),
                             std::move(parent_node_data));
    if (!tree_.Unserialize(tree_update)) {
      LOG(FATAL) << tree_.error();
    }
  }

  if (!unserialized_node_exist) {
    MarkPluginContainerDirty();
  }

  if (ocr_helper_->AreAllPagesOcred()) {
    SetOcrCompleteStatus();
  }
}

void PdfAccessibilityTree::CreateOcrHelper() {
  VLOG(2) << "Creating OCR helper.";
  // If `doc_node_` is not created yet, root id should be sent to `ocr_helper_`
  // when its created.
  auto root_id = doc_node_ ? doc_node_->id : ui::kInvalidAXNodeID;
  ocr_helper_ = std::make_unique<PdfOcrHelper>(
      image_fetcher_, *render_frame_, root_id, page_count_,
      base::BindRepeating(&PdfAccessibilityTree::OnOcrDataReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

bool PdfAccessibilityTree::ShowContextMenu() {
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return false;
  }

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  return obj->PerformAction(action_data);
}

bool PdfAccessibilityTree::SetChildTree(const ui::AXNodeID& target_node_id,
                                        const ui::AXTreeID& child_tree_id) {
  ui::AXNode* target_node = tree_.GetFromId(target_node_id);
  if (!target_node) {
    return false;
  }
  // `nodes_` will be empty once they are unserialized to `tree_`.
  if (!nodes_.empty()) {
    // The `tree_` is not yet fully loaded, thus unable to stitch.
    return false;
  }
  auto obj = GetPluginContainerAXObject();
  if (!obj) {
    return false;
  }
  ui::AXTreeUpdate tree_update;
  ui::AXNodeData target_node_data = target_node->data();
  target_node_data.child_ids = {};
  target_node_data.AddChildTreeId(child_tree_id);
  tree_update.root_id = doc_node_->id;
  tree_update.nodes = {target_node_data};
  CHECK(tree_.Unserialize(tree_update)) << tree_.error();
  MarkPluginContainerDirty();
  return true;
}

void PdfAccessibilityTree::HandleAction(
    const chrome_pdf::AccessibilityActionData& action_data) {
  action_handler_->HandleAccessibilityAction(action_data);
}

std::optional<PdfAccessibilityTree::AnnotationInfo>
PdfAccessibilityTree::GetPdfAnnotationInfoFromAXNode(int32_t ax_node_id) const {
  auto iter = node_id_to_annotation_info_.find(ax_node_id);
  if (iter == node_id_to_annotation_info_.end())
    return std::nullopt;

  return AnnotationInfo(iter->second.page_index, iter->second.annotation_index);
}

void PdfAccessibilityTree::MaybeHandleAccessibilityChange(
    bool always_load_or_reload_accessibility) {
  // This call ensures Blink accessibility always knows about us after it gets
  // created for any reason e.g. mode changes, startup, etc.
  if (UpdateDependentObjects(/*set_this=*/true)) {
    if (always_load_or_reload_accessibility) {
      action_handler_->LoadOrReloadAccessibility();
    } else {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      // Create `ocr_helper_` here when AXMode is set for PDF OCR but
      // `ocr_helper_` has not been created. `ocr_helper_` is supposed to be
      // created upon receiving AXMode with `ui::AXMode::kPDFOcr` above in
      // `AccessibilityModeChanged()`. However, it's possible that
      // `PdfAccessibilityTree` starts observing `content::RenderFrame` after
      // the browser process sent AXMode with `ui::AXMode::kPDFOcr` (i.e. after
      // `RenderAccessibilityManager` called `NotifyAccessibilityModeChange()`)
      // when its web contents were being created.
      // TODO(b/354068257): Enable OCR in print preview mode. OCR is disabled in
      // print preview mode since PDFs are dynamically generated in preview mode
      // and delayed image fetch for OCR is not possible.
      if (!print_preview_ && !ocr_helper_) {
        content::RenderAccessibility* render_accessibility =
            render_frame() ? render_frame()->GetRenderAccessibility() : nullptr;
        if (render_accessibility &&
            render_accessibility->GetAXMode().has_mode(ui::AXMode::kPDFOcr)) {
          CreateOcrHelper();
        }
      }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      action_handler_->EnableAccessibility();
    }
  }
}

void PdfAccessibilityTree::MarkPluginContainerDirty() {
  // Might be nullptr within tests.
  if (!plugin_container_) {
    CHECK_IS_TEST();
    return;
  }

  const blink::WebAXObject& obj =
      blink::WebAXObject::FromWebNode(plugin_container_->GetElement());
  if (obj.IsDetached()) {
    return;
  }

  obj.AddDirtyObjectToSerializationQueue(ax::mojom::EventFrom::kNone,
                                         ax::mojom::Action::kNone,
                                         std::vector<ui::AXEventIntent>());
}

bool PdfAccessibilityTree::UpdateDependentObjects(bool set_this) {
  bool success = true;

  // TODO(accessibility): remove this dependency.
  content::RenderAccessibility* render_accessibility =
      render_frame() ? render_frame()->GetRenderAccessibility() : nullptr;
  if (render_accessibility) {
    render_accessibility->SetPluginAXTreeActionTargetAdapter(
        set_this ? this : nullptr);
  } else {
    success = false;
  }

  auto obj = GetPluginContainerAXObject();
  if (obj && !obj->IsDetached()) {
    obj->SetPluginTreeSource(set_this ? this : nullptr);
  } else {
    success &= !force_plugin_ax_object_for_testing_.IsDetached();
  }

  return success;
}

void PdfAccessibilityTree::ForcePluginAXObjectForTesting(
    const blink::WebAXObject& obj) {
  CHECK_IS_TEST();
  force_plugin_ax_object_for_testing_ = obj;
  UpdateDependentObjects(/*set_this=*/true);
}

}  // namespace pdf
