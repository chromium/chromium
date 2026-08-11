// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/content_phishing_image_embedder.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {

ContentPhishingImageEmbedder::ContentPhishingImageEmbedder(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

ContentPhishingImageEmbedder::~ContentPhishingImageEmbedder() {
  // The RenderView should have called `CancelPendingImageEmbedding()` (which
  // calls `EndTraceEvent()`) before this object is destroyed.
  DCHECK(!is_embedding_running_);
}

void ContentPhishingImageEmbedder::BeginImageEmbedding(
    bool can_extract_visual_features,
    DoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  // Clean up any pending image embedding so that we can start in a known state.
  CancelPendingImageEmbedding();

  is_embedding_running_ = true;
  TRACE_EVENT_BEGIN("safe_browsing", "PhishingImageEmbedding",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::ContentPhishingImageEmbedder", this));

  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();

  PhishingProcessStatus status = CanPerformPhishingDetection(frame);
  switch (status) {
    case PhishingProcessStatus::kInvalidUrlFormat:
      RunFailureCallback(std::move(callback), Result::kInvalidURLFormatRequest);
      return;
    case PhishingProcessStatus::kInvalidDomLoader:
      RunFailureCallback(std::move(callback), Result::kInvalidDocumentLoader);
      return;
    case PhishingProcessStatus::kValid:
      break;
  }

  visual_extractor_ = std::make_unique<PhishingVisualFeatureExtractor>();
  visual_extractor_->ExtractFeatures(
      frame, base::BindOnce(&ContentPhishingImageEmbedder::OnPlaybackDone,
                            weak_factory_.GetWeakPtr(), std::move(callback),
                            can_extract_visual_features));
}

void ContentPhishingImageEmbedder::CancelPendingImageEmbedding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visual_extractor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  PhishingImageEmbedder::CancelPendingImageEmbedding();
}

void ContentPhishingImageEmbedder::OnPlaybackDone(
    DoneCallback callback,
    bool can_extract_visual_features,
    std::unique_ptr<SkBitmap> bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bitmap) {
    BeginImageEmbeddingInternal(gfx::Image::CreateFrom1xBitmap(*bitmap),
                                can_extract_visual_features,
                                std::move(callback));
  } else {
    RunFailureCallback(std::move(callback), Result::kVisualExtractionFailed);
  }
}

void ContentPhishingImageEmbedder::RunFailureCallback(DoneCallback callback,
                                                      Result failure_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  if (callback) {
    std::move(callback).Run(failure_event, ImageFeatureEmbedding(),
                            VisualFeatures());
  }
}

void ContentPhishingImageEmbedder::EndTraceEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_embedding_running_) {
    TRACE_EVENT_END("safe_browsing",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::ContentPhishingImageEmbedder", this));
    is_embedding_running_ = false;
  }
  PhishingImageEmbedder::EndTraceEvent();
}

}  // namespace safe_browsing
