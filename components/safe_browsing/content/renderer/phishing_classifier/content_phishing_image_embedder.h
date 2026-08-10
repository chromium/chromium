// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_IMAGE_EMBEDDER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_IMAGE_EMBEDDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"

class SkBitmap;

namespace content {
class RenderFrame;
}

namespace safe_browsing {
class PhishingVisualFeatureExtractor;

// `ContentPhishingImageEmbedder` is the Blink/content-dependent specialization
// of `PhishingImageEmbedder`. It extends the platform-agnostic image embedding
// engine in `PhishingImageEmbedder` by providing methods to extract visual
// features and screenshots from a `content::RenderFrame`.
//
// This architectural split isolates //content and Blink rendering dependencies
// from //components/safe_browsing/core/common/phishing_classifier, allowing the
// underlying image embedder and scoring logic to be shared across platform
// architectures.
class ContentPhishingImageEmbedder : public PhishingImageEmbedder {
 public:
  using PhishingImageEmbedder::BeginImageEmbedding;

  // Constructs an image embedder bound to the given `render_frame`.
  explicit ContentPhishingImageEmbedder(content::RenderFrame* render_frame);

  ContentPhishingImageEmbedder(const ContentPhishingImageEmbedder&) = delete;
  ContentPhishingImageEmbedder& operator=(const ContentPhishingImageEmbedder&) =
      delete;

  // Destroys the image embedder and cancels any pending embedding.
  ~ContentPhishingImageEmbedder() override;

  // Called by `ContentPhishingImageEmbedderDelegate` once both the page
  // has finished loading/capturing and the browser's `ClientSideDetectionHost`
  // has requested image embedding (after phishing classification or forced
  // request). This begins the visual extraction and image embedding process.
  virtual void BeginImageEmbedding(bool can_extract_visual_features,
                                   DoneCallback callback);

  // Cancels any image embedding that is in progress.
  void CancelPendingImageEmbedding() override;

 private:
  // Callback run when the visual feature extraction (playback) is done.
  // If successful, `bitmap` will be non-null and image embedding will begin.
  void OnPlaybackDone(DoneCallback callback,
                      bool can_extract_visual_features,
                      std::unique_ptr<SkBitmap> bitmap);

  // Helper to run the `DoneCallback` with a failure result.
  void RunFailureCallback(DoneCallback callback, Result failure_event);

  // Helper to end the trace event if one is active.
  void EndTraceEvent() override;

  raw_ptr<content::RenderFrame> render_frame_ = nullptr;  // owns this
  std::unique_ptr<PhishingVisualFeatureExtractor> visual_extractor_;
  bool is_embedding_running_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ContentPhishingImageEmbedder> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_IMAGE_EMBEDDER_H_
