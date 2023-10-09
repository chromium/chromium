// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/renderer/plugin_ax_tree_source.h"
#include "content/public/renderer/render_frame_observer.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_node_data.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PdfOcrRequestStatus {
  kRequested = 0,
  kPerformed = 1,
  kMaxValue = kPerformed,
};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

class PdfAccessibilityTree : public content::PluginAXTreeSource,
                             public content::RenderFrameObserver,
                             public chrome_pdf::PdfAccessibilityDataHandler {
 public:
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Used for storing OCR requests either before performing an OCR job, or after
  // the results have been received. This is for scheduling the work in another
  // task in batches in order to unblock the user from reading a partially
  // OCRed PDF, and in order to avoid sending all the images to the OCR Service
  // at once, in case the PDF is closed halfway through the OCR process.
  struct PdfOcrRequest {
    PdfOcrRequest(const ui::AXNodeID& image_node_id,
                  const chrome_pdf::AccessibilityImageInfo& image,
                  const ui::AXNodeID& parent_node_id,
                  const ui::AXNodeID& page_node_id,
                  uint32_t page_index);

    const ui::AXNodeID image_node_id;
    const chrome_pdf::AccessibilityImageInfo image;
    const ui::AXNodeID parent_node_id;
    const ui::AXNodeID page_node_id;
    const uint32_t page_index;
    // This boolean indicates which request corresponds to the last image on
    // each page.
    bool is_last_on_page = false;

    // This field is set after the image is extracted from PDF.
    gfx::SizeF image_pixel_size;
  };

  // Manages the connection to the OCR Service via Mojo, and ensures that
  // requests are sent in order and that responses are batched.
  class PdfOcrService final {
   public:
    using OnOcrDataReceivedCallback = base::RepeatingCallback<void(
        std::vector<PdfOcrRequest> ocr_requests,
        std::vector<ui::AXTreeUpdate> tree_updates)>;

    PdfOcrService(chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
                  content::RenderFrame& render_frame,
                  uint32_t page_count,
                  OnOcrDataReceivedCallback callback);

    PdfOcrService(const PdfOcrService&) = delete;
    PdfOcrService& operator=(const PdfOcrService&) = delete;

    ~PdfOcrService();

    // If the OCR Service is created before the PDF is loaded or reloaded, i.e.
    // before `PdfAccessibilityTree::SetAccessibilityDocInfo` is called,
    // `PdfAccessibilityTree::remaining_page_count_` would be wrong, hence
    // PdfAccessibilityTree must call this method to keep it up to date.
    void SetPageCount(uint32_t page_count);
    void OcrPage(base::queue<PdfOcrRequest> page_requests);
    bool AreAllPagesOcred() const;
    bool AreAllPagesInBatchOcred() const;
    void SetScreenAIAnnotatorForTesting(
        mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
            screen_ai_annotator);
    void ResetRemainingPageCountForTesting();
    uint32_t pages_per_batch_for_testing() const { return pages_per_batch_; }

   private:
    static uint32_t ComputePagesPerBatch(uint32_t page_count);
    void OcrNextImage();
    void ReceiveOcrResultsForImage(PdfOcrRequest request,
                                   const ui::AXTreeUpdate& tree_update);

    // `image_fetcher_` owns `this`.
    const raw_ptr<chrome_pdf::PdfAccessibilityImageFetcher,
                  ExperimentalRenderer>
        image_fetcher_;

    uint32_t pages_per_batch_;
    uint32_t remaining_page_count_;

    // True if there are pending OCR requests. Used to determine if `OcrPage`
    // should call `OcrNextImage` or if the next call to
    // `ReceiveOcrResultsForImage` should do it instead. This avoids the
    // possibility of processing requests in the wrong order.
    bool is_ocr_in_progress_ = false;
    // A PDF is made up of a number of pages, and each page might have one or
    // more inaccessible images that need to be OCRed. This queue could contain
    // the OCR requests for all the images on several pages, so the requests
    // from each page are concatenated together into a single queue.
    // `PdfOcrRequest.is_last_on_page` indicates which request is the last on
    // each page.
    base::queue<PdfOcrRequest> all_requests_;
    std::vector<PdfOcrRequest> batch_requests_;
    std::vector<ui::AXTreeUpdate> batch_tree_updates_;
    OnOcrDataReceivedCallback on_ocr_data_received_callback_;
    mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;
    SEQUENCE_CHECKER(sequence_checker_);
    // Needs to be kept last so that it would be destructed first.
    base::WeakPtrFactory<PdfOcrService> weak_ptr_factory_{this};
  };
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  PdfAccessibilityTree(
      content::RenderFrame* render_frame,
      chrome_pdf::PdfAccessibilityActionHandler* action_handler,
      chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher);
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
  absl::optional<AnnotationInfo> GetPdfAnnotationInfoFromAXNode(
      int32_t ax_node_id) const;

  // Given the AXNode and the character offset within the AXNode, finds the
  // respective page index and character index within the page. Returns
  // false if the `node` is not a valid static text or inline text box
  // AXNode. Used to find the character offsets of selection.
  bool FindCharacterOffset(
      const ui::AXNode& node,
      uint32_t char_offset_in_node,
      chrome_pdf::PageCharacterIndex& page_char_index) const;

  // content::PluginAXTreeSource:
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
  std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      const ui::AXNode& target_node) override;

  // content::RenderFrameObserver:
  void AccessibilityModeChanged(const ui::AXMode& mode) override;
  void OnDestruct() override;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void CreateOcrService();
  PdfOcrService* ocr_service_for_testing() { return ocr_service_.get(); }

  // After receiving a batch of tree updates containing the results of the OCR
  // Service, this method adds each piece of OCRed text in the correct page,
  // replacing each image node for which we have OCRed text.
  virtual void OnOcrDataReceived(std::vector<PdfOcrRequest> ocr_requests,
                                 std::vector<ui::AXTreeUpdate> tree_updates);

  ui::AXTree& tree_for_testing() { return tree_; }

  const ui::AXTreeUpdate* postamble_page_tree_update_for_testing() const {
    return postamble_page_tree_update_.get();
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  bool ShowContextMenu();

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
 protected:
  // Adds a postample page to the accessibility tree which informs the user that
  // OCR is in progress, if that is indeed the case.
  void AddPostamblePageIfNeeded(const ui::AXNodeID& last_page_node_id);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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

  // Set the status node's message.
  void SetStatusMessage(int message_id);
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

  content::RenderAccessibility* GetRenderAccessibility();

  // WARNING: May cause `this` to be deleted.
  content::RenderAccessibility* GetRenderAccessibilityIfEnabled();

  std::unique_ptr<gfx::Transform> MakeTransformFromViewInfo() const;

  // Handles an accessibility change only if there is a valid
  // `RenderAccessibility` for the frame. `LoadAccessibility()` will be
  // triggered in `PdfViewWebPlugin` when `always_load_or_reload_accessibility`
  // is true, even if the accessibility state is `AccessibilityState::kLoaded`.
  void MaybeHandleAccessibilityChange(bool always_load_or_reload_accessibility);

  // Returns a weak pointer for an instance of this class.
  base::WeakPtr<PdfAccessibilityTree> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  ui::AXTreeData tree_data_;
  ui::AXTree tree_;

  // ‌PdfAccessibilityTree belongs to the PDF plugin which is created by the
  // renderer. `render_frame_` is reset when renderer sends OnDestruct() to its
  // observers.
  raw_ptr<content::RenderFrame, ExperimentalRenderer> render_frame_;

  // Unowned. Must outlive `this`.
  const raw_ptr<chrome_pdf::PdfAccessibilityActionHandler, ExperimentalRenderer>
      action_handler_;
  const raw_ptr<chrome_pdf::PdfAccessibilityImageFetcher, ExperimentalRenderer>
      image_fetcher_;

  // `zoom_` signifies the zoom level set in for the browser content.
  // `scale_` signifies the scale level set by user. Scale is applied
  // by the OS while zoom is applied by the application. Higher scale
  // values are usually set to increase the size of everything on screen.
  // Preferred by people with blurry/low vision. `zoom_` and `scale_`
  // both help us increase/descrease the size of content on screen.
  // From PDF plugin we receive all the data in logical pixels. Which is
  // without the zoom and scale factor applied. We apply the `zoom_` and
  // `scale_` to generate the final bounding boxes of elements in accessibility
  // tree.
  double zoom_ = 1.0;
  double scale_ = 1.0;
  gfx::Vector2dF scroll_;
  gfx::Vector2dF offset_;
  uint32_t selection_start_page_index_ = 0;
  uint32_t selection_start_char_index_ = 0;
  uint32_t selection_end_page_index_ = 0;
  uint32_t selection_end_char_index_ = 0;
  uint32_t page_count_ = 0;
  std::unique_ptr<ui::AXNodeData> doc_node_;
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
  bool sent_metrics_once_ = false;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // The postamble page is added to the accessibility tree to inform the user
  // that the OCR process is ongoing. It is removed once the process is
  // complete.
  std::unique_ptr<ui::AXTreeUpdate> postamble_page_tree_update_;
  // The status node contains a notification message for the user.
  std::unique_ptr<ui::AXNodeData> ocr_status_node_wrapper_;
  std::unique_ptr<ui::AXNodeData> ocr_status_node_;
  std::unique_ptr<PdfOcrService> ocr_service_;

  // Flag indicating if any text was converted from images by OCR.
  bool was_text_converted_from_image_ = false;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  base::WeakPtrFactory<PdfAccessibilityTree> weak_ptr_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
