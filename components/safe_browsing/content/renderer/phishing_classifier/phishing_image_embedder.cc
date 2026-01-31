// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace safe_browsing {

PhishingImageEmbedder::PhishingImageEmbedder(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

bool PhishingImageEmbedder::is_ready() const {
  return !!ScorerStorage::GetInstance()->GetScorer();
}

PhishingImageEmbedder::~PhishingImageEmbedder() {
  DCHECK(done_callback_.is_null());
}

void PhishingImageEmbedder::BeginImageEmbedding(
    bool can_extract_visual_features,
    DoneCallback done_callback) {
  TRACE_EVENT_BEGIN("safe_browsing", "PhishingImageEmbedding",
                    perfetto::Track::FromPointer(this));
  DCHECK(is_ready());

  // However, in an opt build, we will go ahead and clean up the pending
  // image embedding so that we can start in a known state.
  CancelPendingImageEmbedding();

  visual_extractor_ = std::make_unique<PhishingVisualFeatureExtractor>();
  done_callback_ = std::move(done_callback);

  visual_extractor_->ExtractFeatures(
      render_frame_->GetWebFrame(),
      base::BindOnce(&PhishingImageEmbedder::OnPlaybackDone,
                     weak_factory_.GetWeakPtr(), can_extract_visual_features));
}

void PhishingImageEmbedder::OnPlaybackDone(bool can_extract_visual_features,
                                           std::unique_ptr<SkBitmap> bitmap) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (bitmap) {
    bitmap_ = std::move(bitmap);

    ScorerStorage::GetInstance()
        ->GetScorer()
        ->ApplyVisualTfLiteModelImageEmbedding(
            *bitmap_,
            base::BindOnce(&PhishingImageEmbedder::OnImageEmbeddingDone,
                           weak_factory_.GetWeakPtr(),
                           can_extract_visual_features));
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
    bool can_extract_visual_features,
    ImageFeatureEmbedding image_feature_embedding) {
  if (!base::FeatureList::IsEnabled(kClientSideDetectionDeprecateDOMModel) &&
      image_feature_embedding.embedding_value_size() > 0) {
    Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
    image_feature_embedding.set_embedding_model_version(
        scorer->image_embedding_tflite_model_version());
  }

  if (can_extract_visual_features) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&visual_utils::ExtractVisualFeatures, *bitmap_),
        base::BindOnce(&PhishingImageEmbedder::OnVisualFeaturesExtracted,
                       weak_factory_.GetWeakPtr(), image_feature_embedding));
  } else {
    RunCallback(image_feature_embedding, VisualFeatures());
  }
}

void PhishingImageEmbedder::OnVisualFeaturesExtracted(
    ImageFeatureEmbedding image_feature_embedding,
    std::unique_ptr<VisualFeatures> visual_features) {
  RunCallback(image_feature_embedding, *visual_features.get());
}

void PhishingImageEmbedder::RunCallback(
    const ImageFeatureEmbedding& image_feature_embedding,
    const VisualFeatures& visual_features) {
  TRACE_EVENT_END("safe_browsing", /* PhishingImageEmbedding */
                  perfetto::Track::FromPointer(this));
  std::move(done_callback_).Run(image_feature_embedding, visual_features);
  Clear();
}

void PhishingImageEmbedder::RunFailureCallback() {
  RunCallback(ImageFeatureEmbedding(), VisualFeatures());
}

void PhishingImageEmbedder::Clear() {
  done_callback_.Reset();
  bitmap_.reset();
}

}  // namespace safe_browsing
