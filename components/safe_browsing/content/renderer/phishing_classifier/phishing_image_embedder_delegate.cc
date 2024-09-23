// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder_delegate.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using content::RenderThread;

namespace safe_browsing {

namespace {

GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace

PhishingImageEmbedderDelegate::PhishingImageEmbedderDelegate(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      image_embedder_(std::make_unique<PhishingImageEmbedder>(render_frame)),
      is_image_embedding_(false),
      is_image_embedding_running_(false) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::PhishingImageEmbedderDetector>(base::BindRepeating(
          &PhishingImageEmbedderDelegate::PhishingImageEmbedderReceiver,
          base::Unretained(this)));

  model_change_observation_.Observe(ScorerStorage::GetInstance());
}

PhishingImageEmbedderDelegate::~PhishingImageEmbedderDelegate() {
  CancelPendingImageEmbedding(kShutdown);
}

// static
PhishingImageEmbedderDelegate* PhishingImageEmbedderDelegate::Create(
    content::RenderFrame* render_frame) {
  return new PhishingImageEmbedderDelegate(render_frame);
}

void PhishingImageEmbedderDelegate::PhishingImageEmbedderReceiver(
    mojo::PendingAssociatedReceiver<mojom::PhishingImageEmbedderDetector>
        receiver) {
  phishing_image_embedder_receiver_.reset();
  phishing_image_embedder_receiver_.Bind(std::move(receiver));
}

void PhishingImageEmbedderDelegate::StartImageEmbedding(
    const GURL& url,
    StartImageEmbeddingCallback callback) {
  RecordEvent(SBPhishingImageEmbedderEvent::kPhishingImageEmbeddingRequested);
  if (image_embedding_callback_) {
    std::move(image_embedding_callback_)
        .Run(mojom::PhishingImageEmbeddingResult::kCancelled, std::nullopt);
  }
  is_image_embedding_running_ = true;
  last_url_received_from_browser_ = StripRef(url);
  image_embedding_callback_ = std::move(callback);

  MaybeStartImageEmbedding();
}

void PhishingImageEmbedderDelegate::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // A new page is starting to load, so cancel image embedding.
  CancelPendingImageEmbedding(kNavigateAway);
  if (!frame->Parent()) {
    last_main_frame_transition_ = transition;
  }
}

void PhishingImageEmbedderDelegate::DidFinishSameDocumentNavigation() {
  CancelPendingImageEmbedding(kNavigateWithinPage);
}

bool PhishingImageEmbedderDelegate::is_ready() const {
  return image_embedder_->is_ready();
}

void PhishingImageEmbedderDelegate::MaybeStartImageEmbedding() {
  // We can begin the image embedding process when the following conditions are
  // met:
  //  1. A Scorer has been created
  //  2. The browser has sent a StartImageEmbedding message for the
  //     current toplevel URL.
  //  3. The load is a new navigation (not a session history navigation).
  //  4. The toplevel URL has not already been processed.
  if (!image_embedder_->is_ready()) {
    is_image_embedding_running_ = false;
    if (!image_embedding_callback_.is_null()) {
      std::move(image_embedding_callback_)
          .Run(mojom::PhishingImageEmbeddingResult::kImageEmbedderNotReady,
               std::nullopt);
    }
    return;
  }

  if (last_main_frame_transition_ & ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Skip loads from session history navigation.  However, update the
    // last URL sent to the image embedder, so that we'll properly detect
    // same-document navigations.
    last_url_sent_to_image_embedder_ = last_finished_load_url_;
    is_image_embedding_running_ = false;
    if (!image_embedding_callback_.is_null()) {
      std::move(image_embedding_callback_)
          .Run(mojom::PhishingImageEmbeddingResult::kForwardBackTransition,
               std::nullopt);
    }
    return;
  }

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));

  if (last_url_received_from_browser_ != stripped_last_load_url) {
    RecordEvent(
        SBPhishingImageEmbedderEvent::kUrlShouldNotBeUsedForImageEmbedding);
    // The browser has not yet confirmed that this URL should be used for image
    // embedding, so defer image embedding for now.  Note: the ref does not
    // affect any of the browser's pre image embedding checks, so we don't
    // require it to match.
    return;
  }

  last_url_sent_to_image_embedder_ = last_finished_load_url_;
  is_image_embedding_ = true;
  image_embedder_->BeginImageEmbedding(
      base::BindOnce(&PhishingImageEmbedderDelegate::ImageEmbeddingDone,
                     base::Unretained(this)));
}

void PhishingImageEmbedderDelegate::PageCaptured(bool preliminary_capture) {
  RecordEvent(SBPhishingImageEmbedderEvent::kPageTextCaptured);

  if (preliminary_capture) {
    return;
  }
  // Make sure there's no image embedding in progress.  We don't want to swap
  // out the page text string from underneath the term feature extractor.
  //
  // Note: Currently, if the url hasn't changed, we won't restart the image
  // embedding process in this case.  We may want to adjust this.
  CancelPendingImageEmbedding(kPageRecaptured);
  last_finished_load_url_ = render_frame()->GetWebFrame()->GetDocument().Url();

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  // Check if toplevel URL has changed.
  if (stripped_last_load_url == StripRef(last_url_sent_to_image_embedder_)) {
    return;
  }

  MaybeStartImageEmbedding();
}

void PhishingImageEmbedderDelegate::CancelPendingImageEmbedding(
    CancelImageEmbeddingReason reason) {
  if (is_image_embedding_) {
    UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.CancelImageEmbeddingReason",
                              reason, CancelImageEmbeddingReason::kMaxValue);
    is_image_embedding_ = false;
    if (image_embedder_->is_ready()) {
      image_embedder_->CancelPendingImageEmbedding();
    }
  }
}

void PhishingImageEmbedderDelegate::ImageEmbeddingDone(
    const ImageFeatureEmbedding& image_feature_embedding) {
  is_image_embedding_running_ = false;
  if (image_embedding_callback_.is_null()) {
    return;
  }
  if (image_feature_embedding.embedding_value_size()) {
    std::move(image_embedding_callback_)
        .Run(mojom::PhishingImageEmbeddingResult::kSuccess,
             mojo_base::ProtoWrapper(image_feature_embedding));
  } else {
    std::move(image_embedding_callback_)
        .Run(mojom::PhishingImageEmbeddingResult::kFailed, std::nullopt);
  }
}

void PhishingImageEmbedderDelegate::RecordEvent(
    SBPhishingImageEmbedderEvent event) {
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ImageEmbedder.Event", event);
}

void PhishingImageEmbedderDelegate::OnDestruct() {
  if (is_image_embedding_running_) {
    RecordEvent(
        SBPhishingImageEmbedderEvent::kDestructedBeforeImageEmbeddingDone);
  }
  delete this;
}

void PhishingImageEmbedderDelegate::OnScorerChanged() {
  if (is_image_embedding_) {
    CancelPendingImageEmbedding(kNewPhishingScorer);
  }
}

}  // namespace safe_browsing
