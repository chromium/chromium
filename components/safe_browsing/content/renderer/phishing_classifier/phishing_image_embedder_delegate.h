// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_DELEGATE_H_

#include <memory>

#include "base/scoped_observation.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "url/gurl.h"

namespace safe_browsing {
class PhishingImageEmbedder;
class Scorer;

enum class SBPhishingImageEmbedderEvent {
  kPhishingImageEmbeddingRequested = 0,
  // Phishing image embedding could not start because the url was not specified
  // to be processed for image embedding
  kPageTextCaptured = 1,
  kPageTextNotLoaded = 2,
  kUrlShouldNotBeUsedForImageEmbedding = 3,
  // Phishing image embedding could not finish because the class was destructed.
  kDestructedBeforeImageEmbeddingDone = 4,
  kMaxValue = kDestructedBeforeImageEmbeddingDone,
};

// This class is used by the RenderView to interact with a
// PhishingImageEmbedder. This class is self-deleting and has the same lifetime
// as the content::RenderFrame that it is observing.
class PhishingImageEmbedderDelegate
    : public content::RenderFrameObserver,
      public mojom::PhishingImageEmbedderDetector,
      public ScorerStorage::Observer {
 public:
  static PhishingImageEmbedderDelegate* Create(
      content::RenderFrame* render_frame);

  PhishingImageEmbedderDelegate(const PhishingImageEmbedderDelegate&) = delete;
  PhishingImageEmbedderDelegate& operator=(
      const PhishingImageEmbedderDelegate&) = delete;

  ~PhishingImageEmbedderDelegate() override;

  // Called by the RenderFrame when a page has started loading in the given
  // WebFrame.  Typically, this will cause any pending image embedding to be
  // cancelled.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  // Called by the RenderFrame when the same-document navigation has been
  // committed. We continue running the current image embedding.
  void DidFinishSameDocumentNavigation() override;

  // Called by the RenderFrame once a page has finished loading.  Updates the
  // last-loaded URL, then starts image embedding if all other conditions are
  // met (see MaybeStartImageEmbedding for details). We ignore preliminary
  // captures, since these happen before the page has finished loading.
  void PageCaptured(bool preliminary_capture);

  bool is_ready() const;

 private:
  friend class PhishingImageEmbedderDelegateTest;

  explicit PhishingImageEmbedderDelegate(content::RenderFrame* render_frame);

  enum CancelImageEmbeddingReason {
    kNavigateAway,
    kNavigateWithinPage,
    kPageRecaptured,
    kShutdown,
    kNewPhishingScorer,
    kMaxValue = kNewPhishingScorer,
  };

  void PhishingImageEmbedderReceiver(
      mojo::PendingAssociatedReceiver<mojom::PhishingImageEmbedderDetector>
          receiver);

  // Cancels any pending image embedding.
  void CancelPendingImageEmbedding(CancelImageEmbeddingReason reason);

  // Records in UMA of a specific event that happens in the phishing image
  // embedder.
  void RecordEvent(SBPhishingImageEmbedderEvent event);

  void OnDestruct() override;

  // mojom::PhishingImageEmbedding
  // Called by the RenderFrame when it receives a StartImageEmbedding IPC from
  // the browser. This signals that it is okay to begin image embedding for the
  // given toplevel URL. If the URL conditions are fully met once loaded into
  // the RenderFrame and the Scorer has been set with the image embedding model,
  // this will begin the image embedding process, which should only happen once
  // the phishing detection finishes for the same URL and the browser deems the
  // URL to be phishy, or LLAMA forcefully triggers the CSPP ping.
  void StartImageEmbedding(const GURL& url,
                           StartImageEmbeddingCallback callback) override;

  // Called when the image embedding for the current page finishes. This will
  // send a PhishingImageEmbeddingResult::FAILED back to the browser process in
  // one of three scenarios:
  // 1. Visual extraction fails
  // 2. Model TfLite metadata is missing for embedding tflite model dimensions
  // 3. Embedder failed due to embedder creation or process failure.
  void ImageEmbeddingDone(const ImageFeatureEmbedding& image_feature_embedding);

  // Shared code to begin image embedding if all conditions are met.
  void MaybeStartImageEmbedding();

  // ScorerStorage::Observer implementation:
  void OnScorerChanged() override;

  std::unique_ptr<PhishingImageEmbedder> image_embedder_;

  // The last URL that the browser instructed us to process image embedding,
  // with the ref stripped.
  GURL last_url_received_from_browser_;

  // The last top-level URL that has finished loading in the RenderFrame.
  GURL last_finished_load_url_;

  // The transition type for the last load in the main frame.  We use this
  // to exclude back/forward loads from image embedding.  Note that this is
  // set in DidCommitProvisionalLoad(); the transition is reset after this
  // call in the RenderFrame, so we need to save off the value.
  ui::PageTransition last_main_frame_transition_;

  // The URL of the last load that we actually started image embedding on.
  // This is used to suppress phishing image embedding on subframe navigation
  // and back and forward navigations in history.
  GURL last_url_sent_to_image_embedder_;

  // Set to true if the image embedding process in the image embedder is
  // currently running. This boolean variable is used to distinguish whether a
  // image image embedding request or image embedding request arrived.
  bool is_image_embedding_ = false;

  // Set to true when StartPhishingImageEmbedding method is called. It is set
  // to false whenever the image embedding has finished.
  bool is_image_embedding_running_ = false;

  // The callback from the most recent call to StartImageEmbedding.
  StartImageEmbeddingCallback image_embedding_callback_;

  mojo::AssociatedReceiver<mojom::PhishingImageEmbedderDetector>
      phishing_image_embedder_receiver_{this};

  base::ScopedObservation<ScorerStorage, ScorerStorage::Observer>
      model_change_observation_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_DELEGATE_H_
