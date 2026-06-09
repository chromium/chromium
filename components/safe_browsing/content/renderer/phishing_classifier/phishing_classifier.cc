// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "url/gurl.h"

namespace safe_browsing {

const int PhishingClassifier::kClassifierFailed = -1;

PhishingClassifier::PhishingClassifier(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  DCHECK(done_callback_.is_null());
}

bool PhishingClassifier::is_ready() const {
  return !!ScorerStorage::GetInstance()->GetScorer();
}

void PhishingClassifier::SetClientSideDetectionType(
    std::optional<safe_browsing::mojom::ClientSideDetectionType> request_type) {
  request_type_ = request_type;
}

void PhishingClassifier::BeginClassification(DoneCallback done_callback) {
  TRACE_EVENT_BEGIN("safe_browsing", "PhishingClassification",
                    perfetto::Track::FromPointer(this));
  DCHECK(is_ready());

  // However, in an opt build, we will go ahead and clean up the pending
  // classification so that we can start in a known state.
  CancelPendingClassification();

  visual_extractor_ = std::make_unique<PhishingVisualFeatureExtractor>();
  done_callback_ = std::move(done_callback);

  // Cache the URL of the frame right before paint capture for visual
  // extraction. This is needed because the URL of the frame may change
  // after the page has finished loading and during classification via same
  // page navigation.
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  classification_url_ = frame->GetDocument().Url();

  PhishingProcessStatus status = CanPerformPhishingDetection(frame);
  switch (status) {
    case PhishingProcessStatus::kInvalidUrlFormat:
      RunFailureCallback(PhishingClassifier::Result::kInvalidURLFormatRequest);
      return;
    case PhishingProcessStatus::kInvalidDomLoader:
      RunFailureCallback(PhishingClassifier::Result::kInvalidDocumentLoader);
      return;
    case PhishingProcessStatus::kValid:
      break;
  }

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PhishingClassifier::ExtractVisualFeatures,
                                weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::CancelPendingClassification() {
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  DCHECK(is_ready());
  visual_extractor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingClassifier::ExtractVisualFeatures() {
  visual_extractor_->ExtractFeatures(
      render_frame_->GetWebFrame(),
      base::BindOnce(&PhishingClassifier::OnPlaybackDone,
                     base::Unretained(this)));
}

void PhishingClassifier::OnPlaybackDone(std::unique_ptr<SkBitmap> bitmap) {
  if (bitmap) {
    bitmap_ = std::move(bitmap);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&visual_utils::ExtractVisualFeatures, *bitmap_),
        base::BindOnce(&PhishingClassifier::OnVisualFeaturesExtracted,
                       weak_factory_.GetWeakPtr()));
  } else {
    VisualExtractionFinished(/*success=*/false);
  }
}

void PhishingClassifier::OnVisualFeaturesExtracted(
    std::unique_ptr<VisualFeatures> visual_features) {
  visual_features_ = std::move(visual_features);
  VisualExtractionFinished(/*success=*/true);
}

void PhishingClassifier::VisualExtractionFinished(bool success) {
  DCHECK(content::RenderThread::IsMainThread());
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

  ScorerStorage::GetInstance()->GetScorer()->ApplyVisualTfLiteModel(
      *bitmap_, base::BindOnce(&PhishingClassifier::OnVisualTfLiteModelDone,
                               weak_factory_.GetWeakPtr(), std::move(verdict)));
}

void PhishingClassifier::OnVisualTfLiteModelDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::vector<double> result) {
  for (size_t i = 0; i < result.size(); i++) {
    ClientPhishingRequest::CategoryScore* category =
        verdict->add_tflite_model_scores();

    category->set_value(result[i]);
  }

  if (request_type_.has_value() &&
      request_type_.value() ==
          safe_browsing::mojom::ClientSideDetectionType::kImageEmbeddingMatch) {
    ScorerStorage::GetInstance()
        ->GetScorer()
        ->ApplyVisualTfLiteModelImageEmbedding(
            *bitmap_,
            base::BindOnce(
                &PhishingClassifier::OnVisualTfLiteModelImageEmbeddingDone,
                weak_factory_.GetWeakPtr(), std::move(verdict)));
    return;
  }

  RunCallback(*verdict, Result::kSuccess);
}

void PhishingClassifier::OnVisualTfLiteModelImageEmbeddingDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    ImageFeatureEmbedding image_feature_embedding) {
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
  TRACE_EVENT_END("safe_browsing", /* PhishingClassification */
                  perfetto::Track::FromPointer(this));
  std::move(done_callback_).Run(verdict, phishing_classifier_result);
  Clear();
}

void PhishingClassifier::RunFailureCallback(Result failure_event) {
  ClientPhishingRequest verdict;
  // In this case we're not guaranteed to have a valid URL.  Just set it
  // to the empty string to make sure we have a valid protocol buffer.
  verdict.set_url("");
  verdict.set_client_score(kClassifierFailed);
  verdict.set_is_phishing(false);
  RunCallback(verdict, failure_event);
}

void PhishingClassifier::Clear() {
  done_callback_.Reset();
  bitmap_.reset(nullptr);
}

}  // namespace safe_browsing
