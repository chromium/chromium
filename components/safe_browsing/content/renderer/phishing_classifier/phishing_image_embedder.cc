// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder.h"

#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace safe_browsing {

PhishingImageEmbedder::PhishingImageEmbedder(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

bool PhishingImageEmbedder::is_ready() const {
  return !!ScorerStorage::GetInstance()->GetScorer();
}

PhishingImageEmbedder::~PhishingImageEmbedder() {
  DCHECK(done_callback_.is_null());
}

void PhishingImageEmbedder::BeginImageEmbedding(DoneCallback done_callback) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("safe_browsing", "PhishingImageEmbedding",
                                    this);
  DCHECK(is_ready());

  // The RenderView should have CancelPendingImageEmbedding() before calling
  // ImageEmbedding, so DCHECK this.
  DCHECK(done_callback_.is_null());

  // However, in an opt build, we will go ahead and clean up the pending
  // image embedding so that we can start in a known state.
  CancelPendingImageEmbedding();

  visual_extractor_ = std::make_unique<PhishingVisualFeatureExtractor>();
  done_callback_ = std::move(done_callback);

  visual_extractor_->ExtractFeatures(
      render_frame_->GetWebFrame(),
      base::BindOnce(&PhishingImageEmbedder::OnPlaybackDone,
                     weak_factory_.GetWeakPtr()));
}

void PhishingImageEmbedder::OnPlaybackDone(std::unique_ptr<SkBitmap> bitmap) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (bitmap) {
    bitmap_ = std::move(bitmap);
    ScorerStorage::GetInstance()
        ->GetScorer()
        ->ApplyVisualTfLiteModelImageEmbedding(
            *bitmap_,
            base::BindOnce(&PhishingImageEmbedder::OnImageEmbeddingDone,
                           weak_factory_.GetWeakPtr()));
  } else {
    RunFailureCallback();
  }
#else
  RunFailureCallback();
#endif
}

void PhishingImageEmbedder::CancelPendingImageEmbedding() {
  DCHECK(is_ready());
  visual_extractor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingImageEmbedder::OnImageEmbeddingDone(
    ImageFeatureEmbedding image_feature_embedding) {
  if (image_feature_embedding.embedding_value_size() > 0) {
    Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
    image_feature_embedding.set_embedding_model_version(
        scorer->image_embedding_tflite_model_version());
  }
  RunCallback(image_feature_embedding);
}

void PhishingImageEmbedder::RunCallback(
    const ImageFeatureEmbedding& image_feature_embedding) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "PhishingImageEmbedding",
                                  this);
  std::move(done_callback_).Run(image_feature_embedding);
  Clear();
}

void PhishingImageEmbedder::RunFailureCallback() {
  RunCallback(ImageFeatureEmbedding());
}

void PhishingImageEmbedder::Clear() {
  done_callback_.Reset();
  bitmap_.reset();
}

}  // namespace safe_browsing
