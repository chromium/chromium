// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class RenderFrame;
}

namespace safe_browsing {
class ImageFeatureEmbedding;
class PhishingVisualFeatureExtractor;

// This class handles the process of extracting visual features from a page and
// using that to compute a feature vector provided by third party TfLite library
// image_embedder.h
class PhishingImageEmbedder {
 public:
  using DoneCallback =
      base::OnceCallback<void(const ImageFeatureEmbedding& /* verdict */)>;

  explicit PhishingImageEmbedder(content::RenderFrame* render_frame);

  PhishingImageEmbedder(const PhishingImageEmbedder&) = delete;
  PhishingImageEmbedder& operator=(const PhishingImageEmbedder&) = delete;

  virtual ~PhishingImageEmbedder();

  // Returns true if the image embedder is ready to embed pages, i.e. it
  // has had a scorer set via set_phishing_scorer().
  bool is_ready() const;

  // Called by the RenderView when a page has finished loading. This begins the
  // feature extraction used to ultimately create a FrameBuffer object to be
  // used with the ImageEmbedder under the scorer to produce a 1-D feature
  // vector that is to be appended to the ImageFeatureEmbedding message that
  // is passed back to the browser to be added to the CSPP ping.
  virtual void BeginImageEmbedding(DoneCallback callback);

  // Called by the RenderView (on the render thread) when a page is unloading or
  // the RenderView is being destroyed. This cancels any visual feature
  // extraction that is occurring. It is an error if this is called while the
  // image embedder is not ready.
  virtual void CancelPendingImageEmbedding();

 private:
  // Callback when off-thread playback of the recorded paint operations is
  // complete.
  void OnPlaybackDone(std::unique_ptr<SkBitmap> bitmap);

  // Callback when the image embedding feature vector has been added to the
  // verdict.
  void OnImageEmbeddingDone(ImageFeatureEmbedding image_feature_embedding);

  // Helper method to run the Image Embedding process' DoneCallback and clear
  // the state.
  void RunCallback(const ImageFeatureEmbedding& image_feature_embedding);

  // Helper to run the DoneCallback when the visual extraction has failed. This
  // will always send an empty ImageFeatureEmbedding object.
  void RunFailureCallback();

  // Clears the current state of the ImageEmbedder.
  void Clear();

  raw_ptr<content::RenderFrame> render_frame_;  // owns us.
  std::unique_ptr<PhishingVisualFeatureExtractor> visual_extractor_;

  // State for any in-progress image embedding extraction.
  std::unique_ptr<SkBitmap> bitmap_;
  DoneCallback done_callback_;

  // Used in scheduling BeginFeatureExtraction tasks.
  // These pointers are invalidated if image embedding is cancelled.
  base::WeakPtrFactory<PhishingImageEmbedder> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_
