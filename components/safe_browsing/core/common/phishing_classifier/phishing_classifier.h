// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class handles the process of extracting visual features of the page, run
// the image classification model, and send the vector back to browser process
// for computing the phishy score.
//
// For more details, see scorer.h and client_model.proto.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace safe_browsing {
class ClientPhishingRequest;
class VisualFeatures;

class PhishingClassifier {
 public:
  enum class Result {
    kSuccess = 0,
    kInvalidScore = 1,
    kInvalidURLFormatRequest = 2,
    kInvalidDocumentLoader = 3,
    kURLFeatureExtractionFailed = 4,
    kDOMExtractionFailed = 5,
    kTermExtractionFailed = 6,
    kVisualExtractionFailed = 7,
  };

  // Callback to be run when phishing classification finishes. The verdict
  // is a ClientPhishingRequest which contains the verdict computed by the
  // classifier as well as the extracted features.  If the verdict.is_phishing()
  // is true, the page is considered phishy by the client-side model,
  // and the browser should ping back to get a final verdict.  The
  // verdict.client_score() is set to -1 if the classification failed. If the
  // client_score() is not -1, the Result will be kSuccess,
  // and one of other results otherwise.
  typedef base::OnceCallback<void(const ClientPhishingRequest& /* verdict */,
                                  Result /*result*/)>
      DoneCallback;

  static const int kClassifierFailed;

  // Creates a new PhishingClassifier object. Note that the classifier will not
  // be 'ready' until `set_scorer()` is called.
  PhishingClassifier();

  PhishingClassifier(const PhishingClassifier&) = delete;
  PhishingClassifier& operator=(const PhishingClassifier&) = delete;

  virtual ~PhishingClassifier();

  // Returns true if the classifier is ready to classify pages, i.e. it
  // has had a scorer set via `set_scorer()` or `ScorerStorage`.
  bool is_ready() const;

#if BUILDFLAG(IS_IOS)
  // Sets the scorer. Required on iOS because `ScorerStorage` is a process-wide
  // singleton and iOS runs in a multi-Profile browser process architecture.
  void set_scorer(Scorer* scorer);
#endif

  // Begins the phishing classification process for the given URL and image.
  // This method cancels any pending classifications before starting a new one.
  //
  // To avoid blocking the current sequence for too long, phishing
  // classification may run in several chunks of work, posting tasks to continue
  // processing. Once the scoring process is complete, `callback` is run on the
  // current thread.
  //
  // It is an error to call BeginClassification if the classifier is not yet
  // ready.
  virtual void BeginClassification(const GURL& url,
                                   const gfx::Image& image,
                                   DoneCallback callback);

  // Cancels any classification that is in progress.  It is an error to call
  // CancelPendingClassification if the classifier is not yet ready.
  virtual void CancelPendingClassification();

  virtual void SetClientSideDetectionType(
      std::optional<safe_browsing::ClientSideDetectionType> request_type);

 protected:
  // Called by subclasses to end the trace event if one is active.
  virtual void EndTraceEvent();

  // Helper to create a default failure verdict when extraction or setup fails.
  static ClientPhishingRequest CreateFailureVerdict();

  // Internal helper to begin classification without resetting state.
  void BeginClassificationInternal(const GURL& url,
                                   const gfx::Image& image,
                                   DoneCallback done_callback);

 private:
  // Helper to retrieve the active Scorer.
  Scorer* GetScorer() const;

  // Callback when visual features have been extracted from the screenshot.
  void OnVisualFeaturesExtracted(
      std::unique_ptr<VisualFeatures> visual_features);

  // Called when visual extraction is finished.
  // If it was successful, computes a score and runs `done_callback_`.
  // If extraction was unsuccessful, runs `done_callback_` with a
  // non-phishy verdict.
  void VisualExtractionFinished(bool success);

  // Callback when the visual TFLite model has been applied, and returned a list
  // of scores.
  void OnVisualTfLiteModelDone(std::unique_ptr<ClientPhishingRequest> verdict,
                               std::vector<double> result);

  // Callback when the visual TFLite image embedding model has been applied and
  // has returned an ImageFeatureEmbedding.
  void OnVisualTfLiteModelImageEmbeddingDone(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ImageFeatureEmbedding image_feature_embedding);

  // Helper method to run `done_callback_` and clear the state.
  void RunCallback(const ClientPhishingRequest& verdict,
                   Result phishing_classifier_result);

  // Helper to run `done_callback_` when feature extraction has failed.
  // This always signals a non-phishy verdict for the page, with
  // `kClassifierFailed` score.
  void RunFailureCallback(Result failure_event);

  // Clears the current state of the PhishingClassifier.
  void Clear();

  // State for any in-progress extraction.
  gfx::Image image_;
  std::unique_ptr<VisualFeatures> visual_features_;
  DoneCallback done_callback_;

  // Trigger request type forwarded from the
  // `ContentPhishingClassifierDelegate`. Used to determine if the image
  // embedder should be applied after the visual tflite model was applied.
  std::optional<safe_browsing::ClientSideDetectionType> request_type_;

  // The URL of the page being classified. Stored at the beginning of
  // classification to ensure consistency in the verdict.
  GURL classification_url_;

#if BUILDFLAG(IS_IOS)
  // An explicitly set scorer for platforms where ScorerStorage is not used.
  raw_ptr<Scorer> scorer_ = nullptr;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  // Used in scheduling BeginFeatureExtraction tasks.
  // These pointers are invalidated if classification is cancelled.
  base::WeakPtrFactory<PhishingClassifier> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_
