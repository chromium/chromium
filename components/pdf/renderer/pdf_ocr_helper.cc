// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_ocr_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "pdf/pdf_accessibility_image_fetcher.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "ui/accessibility/accessibility_features.h"

namespace pdf {

namespace {

// The delay after which the idle connection to OCR service will be
// disconnected.
constexpr base::TimeDelta kIdleDisconnectDelay = base::Minutes(5);

}  // namespace

//
// PdfOcrRequest
//
PdfOcrRequest::PdfOcrRequest(const ui::AXNodeID& image_node_id,
                             const chrome_pdf::AccessibilityImageInfo& image,
                             const ui::AXNodeID& root_node_id,
                             const ui::AXNodeID& parent_node_id,
                             const ui::AXNodeID& page_node_id,
                             uint32_t page_index)
    : image_node_id(image_node_id),
      image(image),
      root_node_id(root_node_id),
      parent_node_id(parent_node_id),
      page_node_id(page_node_id),
      page_index(page_index) {}

PdfOcrRequest::PdfOcrRequest(const PdfOcrRequest& other) = default;

//
// PdfOcrHelper
//
PdfOcrHelper::PdfOcrHelper(
    chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher,
    content::RenderFrame& render_frame,
    ui::AXNodeID root_node_id,
    uint32_t page_count,
    OnOcrDataReceivedCallback callback)
    : content::RenderFrameObserver(&render_frame),
      image_fetcher_(image_fetcher),
      pages_per_batch_(ComputePagesPerBatch(page_count)),
      remaining_page_count_(page_count),
      root_node_id_(root_node_id),
      on_ocr_data_received_callback_(std::move(callback)) {
  CHECK(features::IsPdfOcrEnabled());
}

PdfOcrHelper::~PdfOcrHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PdfOcrHelper::Reset(ui::AXNodeID root_node_id, uint32_t page_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_GT(page_count, 0u);
  pages_per_batch_ = ComputePagesPerBatch(page_count);
  remaining_page_count_ = page_count;
  root_node_id_ = root_node_id;

  if (!is_ocr_in_progress_) {
    return;
  }

  batch_requests_.clear();
  batch_tree_updates_.clear();
  while (!all_requests_.empty()) {
    all_requests_.pop();
  }

  is_ocr_in_progress_ = false;
}

// static
uint32_t PdfOcrHelper::ComputePagesPerBatch(uint32_t page_count) {
  constexpr uint32_t kMinPagesPerBatch = 1u;
  constexpr uint32_t kMaxPagesPerBatch = 20u;

  return std::clamp<uint32_t>(page_count * 0.1, kMinPagesPerBatch,
                              kMaxPagesPerBatch);
}

void PdfOcrHelper::OcrPage(base::queue<PdfOcrRequest> page_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!page_requests.empty());
  CHECK_GT(remaining_page_count_, 0u);
  page_requests.back().is_last_on_page = true;
  while (!page_requests.empty()) {
    all_requests_.push(page_requests.front());
    page_requests.pop();
  }
  if (!is_ocr_in_progress_) {
    is_ocr_in_progress_ = true;
    OcrNextImage();
  }
}

bool PdfOcrHelper::AreAllPagesOcred() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return remaining_page_count_ == 0u;
}

bool PdfOcrHelper::AreAllPagesInBatchOcred() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AreAllPagesOcred() || remaining_page_count_ % pages_per_batch_ == 0u;
}

void PdfOcrHelper::SetScreenAIAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator) {
  screen_ai_annotator_.reset();
  screen_ai_annotator_.Bind(std::move(screen_ai_annotator));
}

void PdfOcrHelper::ResetRemainingPageCountForTesting() {
  remaining_page_count_ = 0;
}

void PdfOcrHelper::MaybeConnectToOcrService() {
  if (screen_ai_annotator_.is_bound() && screen_ai_annotator_.is_connected()) {
    return;
  }
  if (render_frame()) {
    render_frame()->GetBrowserInterfaceBroker().GetInterface(
        screen_ai_annotator_.BindNewPipeAndPassReceiver());
    screen_ai_annotator_->SetClientType(
        screen_ai::mojom::OcrClientType::kPdfViewer);
    screen_ai_annotator_.reset_on_idle_timeout(kIdleDisconnectDelay);
  }
}

void PdfOcrHelper::OcrNextImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (all_requests_.empty()) {
    return;
  }
  PdfOcrRequest request = all_requests_.front();
  all_requests_.pop();

  SkBitmap bitmap = image_fetcher_->GetImageForOcr(
      request.page_index, request.image.page_object_index);
  request.image_pixel_size = gfx::SizeF(bitmap.width(), bitmap.height());
  if (bitmap.drawsNothing()) {
    ReceiveOcrResultsForImage(std::move(request), ui::AXTreeUpdate());
    return;
  }

  MaybeConnectToOcrService();
  screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
      std::move(bitmap),
      base::BindOnce(&PdfOcrHelper::ReceiveOcrResultsForImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));

  base::UmaHistogramEnumeration("Accessibility.PdfOcr.PDFImages",
                                PdfOcrRequestStatus::kRequested);
}

void PdfOcrHelper::ReceiveOcrResultsForImage(
    PdfOcrRequest request,
    const ui::AXTreeUpdate& tree_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration("Accessibility.PdfOcr.PDFImages",
                                PdfOcrRequestStatus::kPerformed);

  // Ignore the result if the tree has changed.
  if (request.root_node_id != root_node_id_) {
    VLOG(1) << "Tree update for stale tree ignored.";
    return;
  }

  const bool is_last_on_page = request.is_last_on_page;
  batch_requests_.push_back(std::move(request));
  batch_tree_updates_.push_back(tree_update);
  if (is_last_on_page) {
    CHECK_GT(remaining_page_count_, 0u);
    --remaining_page_count_;
    if (AreAllPagesInBatchOcred()) {
      CHECK(!batch_requests_.empty());
      CHECK_EQ(batch_requests_.size(), batch_tree_updates_.size());
      on_ocr_data_received_callback_.Run(std::move(batch_requests_),
                                         std::move(batch_tree_updates_));
    }
  }
  if (all_requests_.empty()) {
    is_ocr_in_progress_ = false;
  } else {
    OcrNextImage();
  }
}

}  // namespace pdf
