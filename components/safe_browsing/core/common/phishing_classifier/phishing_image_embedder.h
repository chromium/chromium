// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {
class Scorer;

// This class handles the process of extracting visual features from a page and
// using that to compute a feature vector provided by third party TfLite library
// image_embedder.h
class PhishingImageEmbedder {
 public:
  enum class Result {
    kSuccess = 0,
    kInvalidURLFormatRequest = 1,
    kInvalidDocumentLoader = 2,
    kVisualExtractionFailed = 3,
  };

  using DoneCallback =
      base::OnceCallback<void(Result /* result */,
                              const ImageFeatureEmbedding& /* verdict */,
                              const VisualFeatures& /* visual_features */)>;

  PhishingImageEmbedder();

  PhishingImageEmbedder(const PhishingImageEmbedder&) = delete;
  PhishingImageEmbedder& operator=(const PhishingImageEmbedder&) = delete;

  virtual ~PhishingImageEmbedder();

  // Returns true if the image embedder is ready to embed pages, i.e. it
  // has had a scorer set via `set_scorer()`.
  bool is_ready() const;

#if BUILDFLAG(IS_IOS)
  // Sets the scorer. Required on iOS because `ScorerStorage` is a process-wide
  // singleton and iOS runs in a multi-Profile browser process architecture.
  void set_scorer(Scorer* scorer);
#endif

  // Begins the image embedding process for the given image.
  //
  // This begins the feature extraction used to ultimately produce a 1-D feature
  // vector that is to be appended to the `ImageFeatureEmbedding` message that
  // is passed back to the browser.
  //
  // This method is the direct entry point for platforms where page snapshots
  // are captured natively outside of Blink (e.g. iOS) as well as unit tests.
  // In Content/Blink, `ContentPhishingImageEmbedder` handles frame capture
  // asynchronously before passing the resulting image into this embedding
  // engine (but through `BeginImageEmbeddingInternal`).
  //
  // It is an error to call `BeginImageEmbedding` if the image embedder is not
  // yet ready.

  virtual void BeginImageEmbedding(const gfx::Image& image,
                                   bool can_extract_visual_features,
                                   DoneCallback callback);

  // Cancels any pending image embedding that is occurring. It is an error if
  // this is called while the image embedder is not ready.
  virtual void CancelPendingImageEmbedding();

 protected:
  // Called by subclasses to end the trace event if one is active.
  virtual void EndTraceEvent();

  // Internal helper to begin image embedding without resetting state. Called
  // directly by `ContentPhishingImageEmbedder` on Content/Blink platforms and
  // by `BeginImageEmbedding` on other platforms (i.e. iOS).
  void BeginImageEmbeddingInternal(const gfx::Image& image,
                                   bool can_extract_visual_features,
                                   DoneCallback done_callback);

 private:
  // Callback when the image embedding feature vector has been added to the
  // verdict.
  void OnImageEmbeddingDone(bool can_extract_visual_features,
                            ImageFeatureEmbedding image_feature_embedding);

  // Callback when visual features have been extracted from the screenshot.
  void OnVisualFeaturesExtracted(
      ImageFeatureEmbedding image_feature_embedding,
      std::unique_ptr<VisualFeatures> visual_features);

  // Helper method to run the Image Embedding process' DoneCallback and clear
  // the state.
  void RunCallback(Result result,
                   const ImageFeatureEmbedding& image_feature_embedding,
                   const VisualFeatures& visual_features);

  // Helper to run the DoneCallback when the visual extraction has failed. This
  // will always send an empty ImageFeatureEmbedding object.
  void RunFailureCallback(Result result);

  // Clears the current state of the ImageEmbedder.
  void Clear();

  // Helper to retrieve the active Scorer.
  Scorer* GetScorer() const;

  // State for any in-progress image embedding extraction.
  gfx::Image image_;
  DoneCallback done_callback_;

#if BUILDFLAG(IS_IOS)
  // An explicitly set scorer for platforms where ScorerStorage is not used.
  raw_ptr<Scorer> scorer_ = nullptr;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  // Used in scheduling BeginImageEmbedding tasks.
  // These pointers are invalidated if image embedding is cancelled.
  base::WeakPtrFactory<PhishingImageEmbedder> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_IMAGE_EMBEDDER_H_
