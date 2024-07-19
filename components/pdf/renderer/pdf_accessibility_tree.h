// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/plugin_ax_tree_action_target_adapter.h"
#include "content/public/renderer/render_frame_observer.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/pdf/renderer/pdf_ocr_helper.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace blink {
class WebPluginContainer;
}  // namespace blink

namespace chrome_pdf {

class PdfAccessibilityActionHandler;
class PdfAccessibilityImageFetcher;

}  // namespace chrome_pdf

namespace content {
class RenderAccessibility;
class RenderFrame;
}  // namespace content

namespace gfx {
class Transform;
}  // namespace gfx

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace pdf {

class PdfAccessibilityTree : public ui::AXTreeSource<const ui::AXNode*,
                                                     ui::AXTreeData*,
                                                     ui::AXNodeData>,
                             public content::PluginAXTreeActionTargetAdapter,
                             public content::RenderFrameObserver,
                             public chrome_pdf::PdfAccessibilityDataHandler {
 public:
  PdfAccessibilityTree(
      content::RenderFrame* render_frame,
      chrome_pdf::PdfAccessibilityActionHandler* action_handler,
      chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
      blink::WebPluginContainer* plugin_container,
      bool print_preview);
  ~PdfAccessibilityTree() override;

  static bool IsDataFromPluginValid(
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects);

  // Stores the page index and annotation index in the page.
  struct AnnotationInfo {
    AnnotationInfo(uint32_t page_index, uint32_t annotation_index);
    AnnotationInfo(const AnnotationInfo& other);
    ~AnnotationInfo();

    uint32_t page_index;
    uint32_t annotation_index;
  };

  // chrome_pdf::PdfAccessibilityDataHandler:
  void SetAccessibilityViewportInfo(
      chrome_pdf::AccessibilityViewportInfo viewport_info) override;
  void SetAccessibilityDocInfo(
      chrome_pdf::AccessibilityDocInfo doc_info) override;
  void SetAccessibilityPageInfo(
      chrome_pdf::AccessibilityPageInfo page_info,
      std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs,
      std::vector<chrome_pdf::AccessibilityCharInfo> chars,
      chrome_pdf::AccessibilityPageObjects page_objects) override;

  void HandleAction(const chrome_pdf::AccessibilityActionData& action_data);
  std::optional<AnnotationInfo> GetPdfAnnotationInfoFromAXNode(
      int32_t ax_node_id) const;

  // Given the AXNode and the character offset within the AXNode, finds the
  // respective page index and character index within the page. Returns
  // false if the `node` is not a valid static text or inline text box
  // AXNode. Used to find the character offsets of selection.
  bool FindCharacterOffset(
      const ui::AXNode& node,
      uint32_t char_offset_in_node,
      chrome_pdf::PageCharacterIndex& page_char_index) const;

  // ui::AXTreeSource:
  bool GetTreeData(ui::AXTreeData* tree_data) const override;
  ui::AXNode* GetRoot() const override;
  ui::AXNode* GetFromId(int32_t id) const override;
  int32_t GetId(const ui::AXNode* node) const override;
  void CacheChildrenIfNeeded(const ui::AXNode*) override {}
  size_t GetChildCount(const ui::AXNode*) const override;
  const ui::AXNode* ChildAt(const ui::AXNode* node, size_t) const override;
  void ClearChildCache(const ui::AXNode*) override {}
  ui::AXNode* GetParent(const ui::AXNode* node) const override;
  bool IsIgnored(const ui::AXNode* node) const override;
  bool IsEqual(const ui::AXNode* node1, const ui::AXNode* node2) const override;
  const ui::AXNode* GetNull() const override;
  void SerializeNode(const ui::AXNode* node,
                     ui::AXNodeData* out_data) const override;

  // content::PluginAXTreeActionTargetAdapter:
  std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      ui::AXNodeID id) override;

  // content::RenderFrameObserver:
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;
  void WasHidden() override;
  void WasShown() override;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void CreateOcrHelper();
  PdfOcrHelper* ocr_helper_for_testing() { return ocr_helper_.get(); }

  // After receiving a batch of tree updates containing the results of the OCR
  // Service, this method adds each piece of OCRed text in the correct page,
  // replacing each image node for which we have OCRed text.
  virtual void OnOcrDataReceived(std::vector<PdfOcrRequest> ocr_requests,
                                 std::vector<ui::AXTreeUpdate> tree_updates);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  bool ShowContextMenu();

  ui::AXTree& tree_for_testing() { return tree_; }

  // Sets the ID of a child tree which this node will be hosting. In this way,
  // multiple trees could be stitched together. Clears any existing descendants
  // of the hosting node in order to maintain the consistency of the tree
  // structure, and because they would be hidden by the child tree anyway.
  bool SetChildTree(const ui::AXNodeID& target_node_id,
                    const ui::AXTreeID& child_tree_id);

  void ForcePluginAXObjectForTesting(const blink::WebAXObject& obj);

 private:
  // Update the AXTreeData when the selected range changed.
  void UpdateAXTreeDataFromSelection();

  void DoSetAccessibilityViewportInfo(
      const chrome_pdf::AccessibilityViewportInfo& viewport_info);
  void DoSetAccessibilityDocInfo(
      const chrome_pdf::AccessibilityDocInfo& doc_info);
  void DoSetAccessibilityPageInfo(
      const chrome_pdf::AccessibilityPageInfo& page_info,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects);

  // Given a 0-based page index and 0-based character index within a page,
  // find the node ID of the associated static text AXNode, and the character
  // index within that text node. Used to find the start and end of the
  // selected text range.
  void FindNodeOffset(uint32_t page_index,
                      uint32_t page_char_index,
                      int32_t* out_node_id,
                      int32_t* out_node_char_index) const;

  // Called after the data for all pages in the PDF have been received.
  // Finishes assembling a complete accessibility tree and grafts it
  // onto the host tree.
  void UnserializeNodes();

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Called after the OCR data for all images in the PDF have been received.
  // Set the status node with the OCR completion message.
  void SetOcrCompleteStatus();
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  void AddPageContent(
      const chrome_pdf::AccessibilityPageInfo& page_info,
      uint32_t page_index,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects);

  // Clears the local cache of node data used to create the tree so that
  // replacement node data can be introduced.
  void ClearAccessibilityNodes();

  std::optional<blink::WebAXObject> GetPluginContainerAXObject();

  std::unique_ptr<gfx::Transform> MakeTransformFromViewInfo() const;

  // Set the status node's message.
  void SetStatusMessage(int message_id);

  void ResetStatusNodeAttributes();

  // Handles an accessibility change only if there is a valid
  // `RenderAccessibility` for the frame. `LoadAccessibility()` will be
  // triggered in `PdfViewWebPlugin` when `always_load_or_reload_accessibility`
  // is true, even if the accessibility state is `AccessibilityState::kLoaded`.
  void MaybeHandleAccessibilityChange(bool always_load_or_reload_accessibility);

  // Marks the plugin container dirty to ensure serialization of the PDF
  // contents.
  void MarkPluginContainerDirty();

  // Let our dependent objects know about our lifetime; `set_this`, if true,
  // sets `this` in our dependents; nullptr otherwise.
  // Returns true on successful update.
  bool UpdateDependentObjects(bool set_this);

  // Returns a weak pointer for an instance of this class.
  base::WeakPtr<PdfAccessibilityTree> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  ui::AXTreeData tree_data_;
  ui::AXTree tree_;

  // ‌PdfAccessibilityTree belongs to the PDF plugin which is created by the
  // renderer. `render_frame_` is reset when renderer sends OnDestruct() to its
  // observers.
  raw_ptr<content::RenderFrame> render_frame_;

  // Unowned. Must outlive `this`.
  const raw_ptr<chrome_pdf::PdfAccessibilityActionHandler> action_handler_;
  const raw_ptr<chrome_pdf::PdfAccessibilityImageFetcher> image_fetcher_;
  const raw_ptr<blink::WebPluginContainer> plugin_container_;

  // `zoom_` signifies the zoom level set in for the browser content.
  // `scale_` signifies the scale level set by user. Scale is applied
  // by the OS while zoom is applied by the application. Higher scale
  // values are usually set to increase the size of everything on screen.
  // Preferred by people with blurry/low vision. `zoom_` and `scale_`
  // both help us increase/descrease the size of content on screen.
  // From PDF plugin we receive all the data in logical pixels. Which is
  // without the zoom and scale factor applied. We apply the `zoom_` and
  // `scale_` to generate the final bounding boxes of elements in accessibility
  // tree. `orientation_` represents page rotations as multiples of 90 degrees,
  // based on `chrome_pdf::PageOrientation`.
  double zoom_ = 1.0;
  double scale_ = 1.0;
  int32_t orientation_ = 0;
  gfx::Vector2dF scroll_;
  gfx::Vector2dF offset_;
  uint32_t selection_start_page_index_ = 0;
  uint32_t selection_start_char_index_ = 0;
  uint32_t selection_end_page_index_ = 0;
  uint32_t selection_end_char_index_ = 0;
  uint32_t page_count_ = 0;
  std::unique_ptr<ui::AXNodeData> doc_node_;
  // The banner node will have an appropriate ARIA landmark for easy navigation
  // for screen reader users. It will contain the status node below.
  std::unique_ptr<ui::AXNodeData> banner_node_;
  // The status node contains a notification message for the user.
  std::unique_ptr<ui::AXNodeData> status_node_;
  std::unique_ptr<ui::AXNodeData> status_node_text_;
  std::vector<std::unique_ptr<ui::AXNodeData>> nodes_;

  // Map from the id of each static text AXNode and inline text box
  // AXNode to the page index and index of the character within its
  // page. Used to find the node associated with the start or end of
  // a selection and vice-versa.
  std::map<int32_t, chrome_pdf::PageCharacterIndex> node_id_to_page_char_index_;

  // Map between AXNode id to annotation object. Used to find the annotation
  // object to which an action can be passed.
  std::map<int32_t, AnnotationInfo> node_id_to_annotation_info_;
  bool invalid_plugin_message_received_ = false;

  // Index of the next expected PDF accessibility page info, used to ignore
  // outdated calls of SetAccessibilityPageInfo().
  uint32_t next_page_index_ = 0;

  bool did_get_a_text_run_ = false;
  bool did_have_an_image_ = false;
  bool sent_metrics_once_ = false;
  // Initialize `currently_in_foreground_` to be true as an associated render
  // frame would be most likely in foreground when being created. If it goes to
  // background, this value will be flipped to false in `WasHidden()`.
  bool currently_in_foreground_ = true;

  // Forces a WebAXObject for the plugin container to be returned, even if the
  // plugin container is nullptr. Enables lower level tests to function.
  blink::WebAXObject force_plugin_ax_object_for_testing_;

  const bool print_preview_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  std::unique_ptr<PdfOcrHelper> ocr_helper_;

  // Flag indicating if any text was converted from images by OCR.
  bool was_text_converted_from_image_ = false;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  base::WeakPtrFactory<PdfAccessibilityTree> weak_ptr_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
