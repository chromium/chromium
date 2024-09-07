// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_OCR_HELPER_H_
#define COMPONENTS_PDF_RENDERER_PDF_OCR_HELPER_H_

#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/accessibility_structs.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update.h"

namespace chrome_pdf {
class PdfAccessibilityImageFetcher;
}  // namespace chrome_pdf

namespace content {
class RenderFrame;
}  // namespace content

namespace pdf {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PdfOcrRequestStatus)
enum class PdfOcrRequestStatus {
  kRequested = 0,
  kPerformed = 1,
  kMaxValue = kPerformed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:PdfOcrRequestStatus)

// Used for storing OCR requests either before performing an OCR job, or after
// the results have been received. This is for scheduling the work in another
// task in batches in order to unblock the user from reading a partially
// OCRed PDF, and in order to avoid sending all the images to the OCR Helper
// at once, in case the PDF is closed halfway through the OCR process.
struct PdfOcrRequest {
  PdfOcrRequest(const ui::AXNodeID& image_node_id,
                const chrome_pdf::AccessibilityImageInfo& image,
                const ui::AXNodeID& root_node_id,
                const ui::AXNodeID& parent_node_id,
                const ui::AXNodeID& page_node_id,
                uint32_t page_index);
  PdfOcrRequest(const PdfOcrRequest& other);

  const ui::AXNodeID image_node_id;
  const chrome_pdf::AccessibilityImageInfo image;
  const ui::AXNodeID root_node_id;
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
class PdfOcrHelper : public content::RenderFrameObserver {
 public:
  using OnOcrDataReceivedCallback =
      base::RepeatingCallback<void(std::vector<PdfOcrRequest> ocr_requests,
                                   std::vector<ui::AXTreeUpdate> tree_updates)>;

  PdfOcrHelper(chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
               content::RenderFrame& render_frame,
               ui::AXNodeID root_node_id,
               uint32_t page_count,
               OnOcrDataReceivedCallback callback);

  PdfOcrHelper(const PdfOcrHelper&) = delete;
  PdfOcrHelper& operator=(const PdfOcrHelper&) = delete;

  ~PdfOcrHelper() override;

  // If the OCR Helper is created before the PDF is loaded or reloaded, i.e.
  // before `PdfAccessibilityTree::SetAccessibilityDocInfo` is called,
  // previous requests are removed and page count and root node are re-set.
  void Reset(ui::AXNodeID root_node_id, uint32_t page_count);
  void OcrPage(base::queue<PdfOcrRequest> page_requests);
  bool AreAllPagesOcred() const;
  bool AreAllPagesInBatchOcred() const;
  void SetScreenAIAnnotatorForTesting(
      mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
          screen_ai_annotator);
  void ResetRemainingPageCountForTesting();
  uint32_t pages_per_batch_for_testing() const { return pages_per_batch_; }

  // content::RenderFrameObserver:
  void OnDestruct() override {}

 private:
  static uint32_t ComputePagesPerBatch(uint32_t page_count);
  void OcrNextImage();
  void ReceiveOcrResultsForImage(PdfOcrRequest request,
                                 const ui::AXTreeUpdate& tree_update);

  // If `screen_ai_annotator_` is not connected to OCR service and
  // `render_frame_` is available, tries to connect it to the OCR service.
  void MaybeConnectToOcrService();

  // `image_fetcher_` owns `this`.
  const raw_ptr<chrome_pdf::PdfAccessibilityImageFetcher> image_fetcher_;

  uint32_t pages_per_batch_;
  uint32_t remaining_page_count_;
  ui::AXNodeID root_node_id_;

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
  base::WeakPtrFactory<PdfOcrHelper> weak_ptr_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_OCR_HELPER_H_
