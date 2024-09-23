// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class handles the process of extracting all of the features from a
// page and computing a phishyness score.  The basic steps are:
//  - Run each feature extractor over the page, building up a FeatureMap of
//    feature -> value.
//  - SHA-256 hash all of the feature names in the map so that they match the
//    supplied model.
//  - Hand the hashed map off to a Scorer, which computes the probability that
//    the page is phishy.
//  - If the page is phishy, run the supplied callback.
//
// For more details, see phishing_*_feature_extractor.h, scorer.h, and
// client_model.proto.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class RenderFrame;
}

namespace safe_browsing {
class ClientPhishingRequest;
class VisualFeatures;
class FeatureMap;
class PhishingDOMFeatureExtractor;
class PhishingTermFeatureExtractor;
class PhishingUrlFeatureExtractor;
class PhishingVisualFeatureExtractor;
class Scorer;

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

  // Creates a new PhishingClassifier object that will operate on
  // |render_view|. Note that the classifier will not be 'ready' until
  // set_phishing_scorer() is called.
  explicit PhishingClassifier(content::RenderFrame* render_frame);

  PhishingClassifier(const PhishingClassifier&) = delete;
  PhishingClassifier& operator=(const PhishingClassifier&) = delete;

  virtual ~PhishingClassifier();

  // Returns true if the classifier is ready to classify pages, i.e. it
  // has had a scorer set via set_phishing_scorer().
  bool is_ready() const;

  // Called by the RenderView when a page has finished loading.  This begins
  // the feature extraction and scoring process. |page_text| should contain
  // the plain text of a web page, including any subframes, as returned by
  // RenderView::CaptureText().  |page_text| is owned by the caller, and must
  // not be destroyed until either |done_callback| is run or
  // CancelPendingClassification() is called.
  //
  // To avoid blocking the render thread for too long, phishing classification
  // may run in several chunks of work, posting a task to the current
  // MessageLoop to continue processing.  Once the scoring process is complete,
  // |done_callback| is run on the current thread.  PhishingClassifier takes
  // ownership of the callback.
  //
  // It is an error to call BeginClassification if the classifier is not yet
  // ready.
  virtual void BeginClassification(
      scoped_refptr<const base::RefCountedString16> page_text,
      DoneCallback callback);

  // Called by the RenderView (on the render thread) when a page is unloading
  // or the RenderView is being destroyed.  This cancels any extraction that
  // is in progress.  It is an error to call CancelPendingClassification if
  // the classifier is not yet ready.
  virtual void CancelPendingClassification();

 private:
  // Any score equal to or above this value is considered phishy.
  static const float kPhishyThreshold;

  // Begins the feature extraction process, by extracting URL features and
  // beginning DOM feature extraction.
  void BeginFeatureExtraction();

  // Callback to be run when DOM feature extraction is complete.
  // If it was successful, begins term feature extraction, otherwise
  // runs the DoneCallback with a non-phishy verdict.
  void DOMExtractionFinished(bool success);

  // Callback to be run when term feature extraction is complete.
  // If it was successful, begins visual feature extraction, otherwise runs the
  // DoneCallback with a non-phishy verdict.
  void TermExtractionFinished(bool success);

  // Called to extract the visual features of the current page.
  void ExtractVisualFeatures();

  // Callback when off-thread playback of the recorded paint operations is
  // complete.
  void OnPlaybackDone(std::unique_ptr<SkBitmap> bitmap);

  // Callback when visual features have been extracted from the screenshot.
  void OnVisualFeaturesExtracted(
      std::unique_ptr<VisualFeatures> visual_features);

  // Callback when visual feature extraction is complete.
  // If it was successful, computes a score and runs the DoneCallback.
  // If extraction was unsuccessful, runs the DoneCallback with a
  // non-phishy verdict.
  void VisualExtractionFinished(bool success);

  // Callback when the visual TFLite model has been applied, and returned a list
  // of scores.
  void OnVisualTfLiteModelDone(std::unique_ptr<ClientPhishingRequest> verdict,
                               std::vector<double> result);

  // Helper method to run the DoneCallback and clear the state.
  void RunCallback(const ClientPhishingRequest& verdict,
                   Result phishing_classifier_result);

  // Helper to run the DoneCallback when feature extraction has failed.
  // This always signals a non-phishy verdict for the page, with
  // |kInvalidScore|.
  void RunFailureCallback(Result failure_event);

  // Clears the current state of the PhishingClassifier.
  void Clear();

  raw_ptr<content::RenderFrame, DanglingUntriaged> render_frame_;  // owns us
  std::unique_ptr<PhishingUrlFeatureExtractor> url_extractor_;
  std::unique_ptr<PhishingDOMFeatureExtractor> dom_extractor_;
  std::unique_ptr<PhishingTermFeatureExtractor> term_extractor_;
  std::unique_ptr<PhishingVisualFeatureExtractor> visual_extractor_;

  // State for any in-progress extraction.
  std::unique_ptr<FeatureMap> features_;
  std::unique_ptr<std::set<uint32_t>> shingle_hashes_;
  scoped_refptr<const base::RefCountedString16> page_text_;
  std::unique_ptr<SkBitmap> bitmap_;
  std::unique_ptr<VisualFeatures> visual_features_;
  DoneCallback done_callback_;

  // Used to record the duration of visual feature scoring.
  base::TimeTicks visual_matching_start_;

  // Used in scheduling BeginFeatureExtraction tasks.
  // These pointers are invalidated if classification is cancelled.
  base::WeakPtrFactory<PhishingClassifier> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_CLASSIFIER_H_
