// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"

#include <utility>

#include "base/check.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {

PhishingImageEmbedder::PhishingImageEmbedder() = default;

Scorer* PhishingImageEmbedder::GetScorer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_IOS)
  return scorer_;
#else
  return ScorerStorage::GetInstance()->GetScorer();
#endif
}

bool PhishingImageEmbedder::is_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!GetScorer();
}

#if BUILDFLAG(IS_IOS)
void PhishingImageEmbedder::set_scorer(Scorer* scorer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scorer_ = scorer;
}
#endif

PhishingImageEmbedder::~PhishingImageEmbedder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_callback_.is_null());
}

void PhishingImageEmbedder::BeginImageEmbedding(
    const gfx::Image& image,
    bool can_extract_visual_features,
    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  // Clean up any pending image embedding so that we can start in a known state.
  CancelPendingImageEmbedding();

  BeginImageEmbeddingInternal(image, can_extract_visual_features,
                              std::move(done_callback));
}

void PhishingImageEmbedder::BeginImageEmbeddingInternal(
    const gfx::Image& image,
    bool can_extract_visual_features,
    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  TRACE_EVENT_BEGIN("safe_browsing", "PhishingImageEmbedding",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::PhishingImageEmbedder", this));

  image_ = image;
  done_callback_ = std::move(done_callback);

  Scorer* scorer = GetScorer();
  DCHECK(scorer);

  scorer->ApplyVisualTfLiteModelImageEmbedding(
      image_,
      base::BindOnce(&PhishingImageEmbedder::OnImageEmbeddingDone,
                     weak_factory_.GetWeakPtr(), can_extract_visual_features));
}

void PhishingImageEmbedder::EndTraceEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!done_callback_.is_null()) {
    TRACE_EVENT_END("safe_browsing",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::PhishingImageEmbedder", this));
  }
}

void PhishingImageEmbedder::CancelPendingImageEmbedding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingImageEmbedder::OnImageEmbeddingDone(
    bool can_extract_visual_features,
    ImageFeatureEmbedding image_feature_embedding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (can_extract_visual_features) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&visual_utils::ExtractVisualFeatures,
                       visual_utils::GetBitmapForVisualFeatures(image_)),
        base::BindOnce(&PhishingImageEmbedder::OnVisualFeaturesExtracted,
                       weak_factory_.GetWeakPtr(),
                       std::move(image_feature_embedding)));
  } else {
    RunCallback(Result::kSuccess, image_feature_embedding, VisualFeatures());
  }
}

void PhishingImageEmbedder::OnVisualFeaturesExtracted(
    ImageFeatureEmbedding image_feature_embedding,
    std::unique_ptr<VisualFeatures> visual_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunCallback(Result::kSuccess, image_feature_embedding,
              *visual_features.get());
}

void PhishingImageEmbedder::RunCallback(
    Result result,
    const ImageFeatureEmbedding& image_feature_embedding,
    const VisualFeatures& visual_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  std::move(done_callback_)
      .Run(result, image_feature_embedding, visual_features);
  Clear();
}

void PhishingImageEmbedder::RunFailureCallback(Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunCallback(result, ImageFeatureEmbedding(), VisualFeatures());
}

void PhishingImageEmbedder::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  done_callback_.Reset();
  image_ = gfx::Image();
}

}  // namespace safe_browsing
