// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace safe_browsing {

const int PhishingClassifier::kClassifierFailed = -1;

PhishingClassifier::PhishingClassifier() {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  DCHECK(done_callback_.is_null());
}

Scorer* PhishingClassifier::GetScorer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_IOS)
  return scorer_;
#else
  return ScorerStorage::GetInstance()->GetScorer();
#endif
}

bool PhishingClassifier::is_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!GetScorer();
}

#if BUILDFLAG(IS_IOS)
void PhishingClassifier::set_scorer(Scorer* scorer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scorer_ = scorer;
}
#endif

void PhishingClassifier::SetClientSideDetectionType(
    std::optional<safe_browsing::ClientSideDetectionType> request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_type_ = request_type;
}

void PhishingClassifier::BeginClassification(const GURL& url,
                                             const gfx::Image& image,
                                             DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  // Clean up any pending classification so that we can start in a known state.
  CancelPendingClassification();

  BeginClassificationInternal(url, image, std::move(done_callback));
}

void PhishingClassifier::BeginClassificationInternal(
    const GURL& url,
    const gfx::Image& image,
    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  TRACE_EVENT_BEGIN("safe_browsing", "PhishingClassification",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::PhishingClassifier", this));

  done_callback_ = std::move(done_callback);
  classification_url_ = url;
  image_ = image;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&visual_utils::ExtractVisualFeatures,
                     visual_utils::GetBitmapForVisualFeatures(image_)),
      base::BindOnce(&PhishingClassifier::OnVisualFeaturesExtracted,
                     weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::CancelPendingClassification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingClassifier::OnVisualFeaturesExtracted(
    std::unique_ptr<VisualFeatures> visual_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visual_features_ = std::move(visual_features);
  VisualExtractionFinished(/*success=*/true);
}

void PhishingClassifier::VisualExtractionFinished(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    RunFailureCallback(Result::kVisualExtractionFailed);
    return;
  }

  std::unique_ptr<ClientPhishingRequest> verdict =
      std::make_unique<ClientPhishingRequest>();
  verdict->set_url(classification_url_.spec());
  // Because the client_score is required, set a dummy value so that it can be
  // parsed in the browser host class.
  verdict->set_client_score(0);

  if (visual_features_) {
    verdict->mutable_visual_features()->Swap(visual_features_.get());
  }

  Scorer* scorer = GetScorer();
  DCHECK(scorer);
  scorer->ApplyVisualTfLiteModel(
      image_, base::BindOnce(&PhishingClassifier::OnVisualTfLiteModelDone,
                             weak_factory_.GetWeakPtr(), std::move(verdict)));
}

void PhishingClassifier::OnVisualTfLiteModelDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::vector<double> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < result.size(); i++) {
    ClientPhishingRequest::CategoryScore* category =
        verdict->add_tflite_model_scores();

    category->set_value(result[i]);
  }

  if (request_type_.has_value() &&
      request_type_.value() ==
          safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH) {
    Scorer* scorer = GetScorer();
    DCHECK(scorer);
    scorer->ApplyVisualTfLiteModelImageEmbedding(
        image_, base::BindOnce(
                    &PhishingClassifier::OnVisualTfLiteModelImageEmbeddingDone,
                    weak_factory_.GetWeakPtr(), std::move(verdict)));
    return;
  }

  RunCallback(*verdict, Result::kSuccess);
}

void PhishingClassifier::OnVisualTfLiteModelImageEmbeddingDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    ImageFeatureEmbedding image_feature_embedding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool has_image_feature_embedding =
      image_feature_embedding.embedding_value_size() > 0;
  if (has_image_feature_embedding) {
    *verdict->mutable_image_feature_embedding() = image_feature_embedding;
  }
  base::UmaHistogramBoolean(
      "SBClientPhishing.ImageEmbedding.CapturedWithPhishingClassification",
      has_image_feature_embedding);
  RunCallback(*verdict, Result::kSuccess);
}

void PhishingClassifier::RunCallback(const ClientPhishingRequest& verdict,
                                     Result phishing_classifier_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  std::move(done_callback_).Run(verdict, phishing_classifier_result);
  Clear();
}

void PhishingClassifier::RunFailureCallback(Result failure_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunCallback(CreateFailureVerdict(), failure_event);
}

// static
ClientPhishingRequest PhishingClassifier::CreateFailureVerdict() {
  ClientPhishingRequest verdict;
  // In this case we're not guaranteed to have a valid URL.  Just set it
  // to the empty string to make sure we have a valid protocol buffer.
  verdict.set_url("");
  verdict.set_client_score(kClassifierFailed);
  verdict.set_is_phishing(false);
  return verdict;
}

void PhishingClassifier::EndTraceEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!done_callback_.is_null()) {
    TRACE_EVENT_END("safe_browsing", /* PhishingClassification */
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::PhishingClassifier", this));
  }
}

void PhishingClassifier::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  done_callback_.Reset();
  visual_features_.reset();
  image_ = gfx::Image();
}

}  // namespace safe_browsing
