// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/content_phishing_classifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_utils.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {

ContentPhishingClassifier::ContentPhishingClassifier(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

ContentPhishingClassifier::~ContentPhishingClassifier() {
  // The RenderView should have called `CancelPendingClassification()` (which
  // calls `EndTraceEvent()`) before this object is destroyed.
  DCHECK(!is_classifying_);
}

void ContentPhishingClassifier::BeginClassification(DoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_ready());

  // Clean up any pending classification so that we can start in a known state.
  CancelPendingClassification();

  is_classifying_ = true;
  TRACE_EVENT_BEGIN("safe_browsing", "PhishingClassification",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::ContentPhishingClassifier", this));

  visual_extractor_ = std::make_unique<PhishingVisualFeatureExtractor>();

  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  GURL classification_url = frame->GetDocument().Url();

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

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContentPhishingClassifier::ExtractVisualFeatures,
                     weak_factory_.GetWeakPtr(), std::move(classification_url),
                     std::move(callback)));
}

void ContentPhishingClassifier::CancelPendingClassification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  visual_extractor_.reset();
  weak_factory_.InvalidateWeakPtrs();
  PhishingClassifier::CancelPendingClassification();
}

void ContentPhishingClassifier::ExtractVisualFeatures(GURL classification_url,
                                                      DoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visual_extractor_->ExtractFeatures(
      render_frame_->GetWebFrame(),
      base::BindOnce(&ContentPhishingClassifier::OnPlaybackDone,
                     weak_factory_.GetWeakPtr(), std::move(classification_url),
                     std::move(callback)));
}

void ContentPhishingClassifier::OnPlaybackDone(
    GURL classification_url,
    DoneCallback callback,
    std::unique_ptr<SkBitmap> bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bitmap) {
    BeginClassificationInternal(classification_url,
                                gfx::Image::CreateFrom1xBitmap(*bitmap),
                                std::move(callback));
  } else {
    RunFailureCallback(std::move(callback), Result::kVisualExtractionFailed);
  }
}

void ContentPhishingClassifier::RunFailureCallback(DoneCallback callback,
                                                   Result failure_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndTraceEvent();
  std::move(callback).Run(PhishingClassifier::CreateFailureVerdict(),
                          failure_event);
}

void ContentPhishingClassifier::EndTraceEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PhishingClassifier::EndTraceEvent();
  if (is_classifying_) {
    is_classifying_ = false;
    TRACE_EVENT_END("safe_browsing",
                    perfetto::NamedTrack::FromPointer(
                        "safe_browsing::ContentPhishingClassifier", this));
  }
}

}  // namespace safe_browsing
