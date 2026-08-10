// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_CLASSIFIER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_CLASSIFIER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}

namespace safe_browsing {
class PhishingVisualFeatureExtractor;

// `ContentPhishingClassifier` is the Blink/content-dependent specialization of
// `PhishingClassifier`. It extends the platform-agnostic scoring logic in
// `PhishingClassifier` by providing methods to extract visual
// features from a `content::RenderFrame`.
//
// This separation allows the core classification engine in
// //components/safe_browsing/core/common/phishing_classifier to remain pure C++
// and platform-agnostic (reusable across iOS and non-iOS), while this wrapper
// isolates all dependencies on //content and Blink rendering structures.
class ContentPhishingClassifier : public PhishingClassifier {
 public:
  using PhishingClassifier::BeginClassification;

  // Constructs a classifier bound to the given |render_frame|.
  explicit ContentPhishingClassifier(content::RenderFrame* render_frame);

  ContentPhishingClassifier(const ContentPhishingClassifier&) = delete;
  ContentPhishingClassifier& operator=(const ContentPhishingClassifier&) =
      delete;

  // Destroys the classifier and cancels any pending classification.
  ~ContentPhishingClassifier() override;

  // Called by the `ContentPhishingClassifierDelegate` when a page has finished
  // loading. This begins the visual extraction and scoring process.
  virtual void BeginClassification(DoneCallback callback);

  // Cancels any extraction that is in progress.
  void CancelPendingClassification() override;

 private:
  // Begins the visual feature extraction process using the visual extractor.
  void ExtractVisualFeatures(GURL classification_url, DoneCallback callback);

  // Callback run when the visual feature extraction (playback) is done.
  // If successful, `bitmap` will be non-null and classification will begin.
  void OnPlaybackDone(GURL classification_url,
                      DoneCallback callback,
                      std::unique_ptr<SkBitmap> bitmap);

  // Helper to run the `DoneCallback` with a failure result.
  void RunFailureCallback(DoneCallback callback, Result failure_event);

  // Helper to end the trace event if one is active.
  void EndTraceEvent() override;

  raw_ptr<content::RenderFrame> render_frame_ = nullptr;  // owns this
  std::unique_ptr<PhishingVisualFeatureExtractor> visual_extractor_;
  bool is_classifying_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ContentPhishingClassifier> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_CONTENT_PHISHING_CLASSIFIER_H_
