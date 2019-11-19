// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_url_feature_extractor.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"

namespace safe_browsing {

const float PhishingClassifier::kInvalidScore = -1.0;
const float PhishingClassifier::kPhishyThreshold = 0.5;

PhishingClassifier::PhishingClassifier(content::RenderFrame* render_frame,
                                       FeatureExtractorClock* clock)
    : render_frame_(render_frame), scorer_(nullptr), clock_(clock) {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  CheckNoPendingClassification();
}

void PhishingClassifier::set_phishing_scorer(const Scorer* scorer) {
  CheckNoPendingClassification();
  scorer_ = scorer;
  if (scorer_) {
    url_extractor_.reset(new PhishingUrlFeatureExtractor);
    dom_extractor_.reset(new PhishingDOMFeatureExtractor(clock_.get()));
    term_extractor_.reset(new PhishingTermFeatureExtractor(
        &scorer_->page_terms(),
        &scorer_->page_words(),
        scorer_->max_words_per_term(),
        scorer_->murmurhash3_seed(),
        scorer_->max_shingles_per_page(),
        scorer_->shingle_size(),
        clock_.get()));
  } else {
    // We're disabling client-side phishing detection, so tear down all
    // of the relevant objects.
    url_extractor_.reset();
    dom_extractor_.reset();
    term_extractor_.reset();
  }
}

bool PhishingClassifier::is_ready() const {
  return scorer_ != NULL;
}

void PhishingClassifier::BeginClassification(const base::string16* page_text,
                                             DoneCallback done_callback) {
  DCHECK(is_ready());

  // The RenderView should have called CancelPendingClassification() before
  // starting a new classification, so DCHECK this.
  CheckNoPendingClassification();
  // However, in an opt build, we will go ahead and clean up the pending
  // classification so that we can start in a known state.
  CancelPendingClassification();

  page_text_ = page_text;
  done_callback_ = std::move(done_callback);

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PhishingClassifier::BeginFeatureExtraction,
                                weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::BeginFeatureExtraction() {
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();

  // Check whether the URL is one that we should classify.
  // Currently, we only classify http/https URLs that are GET requests.
  GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    RunFailureCallback();
    return;
  }

  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (!document_loader || document_loader->HttpMethod().Ascii() != "GET") {
    RunFailureCallback();
    return;
  }

  features_.reset(new FeatureMap);
  if (!url_extractor_->ExtractFeatures(url, features_.get())) {
    RunFailureCallback();
    return;
  }

  // DOM feature extraction can take awhile, so it runs asynchronously
  // in several chunks of work and invokes the callback when finished.
  dom_extractor_->ExtractFeatures(
      frame->GetDocument(), features_.get(),
      base::BindOnce(&PhishingClassifier::DOMExtractionFinished,
                     base::Unretained(this)));
}

void PhishingClassifier::CancelPendingClassification() {
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  DCHECK(is_ready());
  dom_extractor_->CancelPendingExtraction();
  term_extractor_->CancelPendingExtraction();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingClassifier::DOMExtractionFinished(bool success) {
  shingle_hashes_.reset(new std::set<uint32_t>);
  if (success) {
    // Term feature extraction can take awhile, so it runs asynchronously
    // in several chunks of work and invokes the callback when finished.
    term_extractor_->ExtractFeatures(
        page_text_, features_.get(), shingle_hashes_.get(),
        base::BindOnce(&PhishingClassifier::TermExtractionFinished,
                       base::Unretained(this)));
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::TermExtractionFinished(bool success) {
  if (success) {
    blink::WebLocalFrame* main_frame = render_frame_->GetWebFrame();

    // Hash all of the features so that they match the model, then compute
    // the score.
    FeatureMap hashed_features;
    ClientPhishingRequest verdict;
    verdict.set_model_version(scorer_->model_version());
    verdict.set_url(main_frame->GetDocument().Url().GetString().Utf8());
    for (const auto& it : features_->features()) {
      bool result = hashed_features.AddRealFeature(
          crypto::SHA256HashString(it.first), it.second);
      DCHECK(result);
      ClientPhishingRequest::Feature* feature = verdict.add_feature_map();
      feature->set_name(it.first);
      feature->set_value(it.second);
    }
    for (const auto& it : *shingle_hashes_) {
      verdict.add_shingle_hashes(it);
    }
    float score = static_cast<float>(scorer_->ComputeScore(hashed_features));
    verdict.set_client_score(score);
    verdict.set_is_phishing(score >= kPhishyThreshold);
    RunCallback(verdict);
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::CheckNoPendingClassification() {
  DCHECK(done_callback_.is_null());
  DCHECK(!page_text_);
  if (!done_callback_.is_null() || page_text_) {
    LOG(ERROR) << "Classification in progress, missing call to "
               << "CancelPendingClassification";
  }
}

void PhishingClassifier::RunCallback(const ClientPhishingRequest& verdict) {
  std::move(done_callback_).Run(verdict);
  Clear();
}

void PhishingClassifier::RunFailureCallback() {
  ClientPhishingRequest verdict;
  // In this case we're not guaranteed to have a valid URL.  Just set it
  // to the empty string to make sure we have a valid protocol buffer.
  verdict.set_url("");
  verdict.set_client_score(kInvalidScore);
  verdict.set_is_phishing(false);
  RunCallback(verdict);
}

void PhishingClassifier::Clear() {
  page_text_ = NULL;
  done_callback_.Reset();
  features_.reset(NULL);
  shingle_hashes_.reset(NULL);
}

}  // namespace safe_browsing
