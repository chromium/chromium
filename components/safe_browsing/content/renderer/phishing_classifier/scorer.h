// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This abstract class loads a client-side model and lets you compute a phishing
// score for a set of previously extracted features.  The phishing score
// corresponds to the probability that the features are indicative of a phishing
// site.
//
// For more details on how the score is actually computed, consult the two
// derived classes protobuf_scorer.h and flatbuffer_scorer.h
//
// See features.h for a list of features that are currently used.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
class FeatureMap;

// Enum used to keep stats about the status of the Scorer creation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum ScorerCreationStatus {
  SCORER_SUCCESS = 0,
  SCORER_FAIL_MODEL_OPEN_FAIL = 1,       // Not used anymore
  SCORER_FAIL_MODEL_FILE_EMPTY = 2,      // Not used anymore
  SCORER_FAIL_MODEL_FILE_TOO_LARGE = 3,  // Not used anymore
  SCORER_FAIL_MODEL_PARSE_ERROR = 4,
  SCORER_FAIL_MODEL_MISSING_FIELDS = 5,
  SCORER_FAIL_MAP_VISUAL_TFLITE_MODEL = 6,
  SCORER_FAIL_FLATBUFFER_INVALID_REGION = 7,
  SCORER_FAIL_FLATBUFFER_INVALID_MAPPING = 8,
  SCORER_FAIL_FLATBUFFER_FAILED_VERIFY = 9,
  SCORER_FAIL_FLATBUFFER_BAD_INDICES_OR_FIELDS = 10,
  SCORER_STATUS_MAX  // Always add new values before this one.
};

// Scorer methods are virtual to simplify mocking of this class,
// and to allow inheritance.
class Scorer {
 public:
  virtual ~Scorer();
  // Most clients should use the factory method.  This constructor is public
  // to allow for mock implementations.
  Scorer();

  // This method computes the probability that the given features are indicative
  // of phishing.  It returns a score value that falls in the range [0.0,1.0]
  // (range is inclusive on both ends).
  virtual double ComputeScore(const FeatureMap& features) const = 0;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // This method applies the TfLite visual model to the given bitmap. It
  // asynchronously returns the list of scores for each category, in the same
  // order as `tflite_thresholds()`.
  virtual void ApplyVisualTfLiteModel(
      const SkBitmap& bitmap,
      base::OnceCallback<void(std::vector<double>)> callback) const = 0;
#endif

  // Returns the version number of the loaded client model.
  virtual int model_version() const = 0;

  virtual int dom_model_version() const = 0;

  bool HasVisualTfLiteModel() const;

  // -- Accessors used by the page feature extractor ---------------------------

  // Returns a callback to find if a page word is in the model.
  virtual base::RepeatingCallback<bool(uint32_t)> find_page_word_callback()
      const = 0;

  // Returns a callback to find if a page term is in the model.
  virtual base::RepeatingCallback<bool(const std::string&)>
  find_page_term_callback() const = 0;

  // Return the maximum number of words per term for the loaded model.
  virtual size_t max_words_per_term() const = 0;

  // Returns the murmurhash3 seed for the loaded model.
  virtual uint32_t murmurhash3_seed() const = 0;

  // Return the maximum number of unique shingle hashes per page.
  virtual size_t max_shingles_per_page() const = 0;

  // Return the number of words in a shingle.
  virtual size_t shingle_size() const = 0;

  // Returns the threshold probability above which we send a CSD ping.
  virtual float threshold_probability() const = 0;

  // Returns the version of the visual TFLite model.
  virtual int tflite_model_version() const = 0;

  // Returns the thresholds configured for the visual TFLite model categories.
  virtual const google::protobuf::RepeatedPtrField<
      TfLiteModelMetadata::Threshold>&
  tflite_thresholds() const = 0;

  // Disable copy and move.
  Scorer(const Scorer&) = delete;
  Scorer& operator=(const Scorer&) = delete;

 protected:
  // Helper function which converts log odds to a probability in the range
  // [0.0,1.0].
  static double LogOdds2Prob(double log_odds);

  // Apply the tflite model to the bitmap. The scores are returned by running
  // `callback` on the provided `callback_task_runner`. This is expected to be
  // run on a helper thread.
  static void ApplyVisualTfLiteModelHelper(
      const SkBitmap& bitmap,
      int input_width,
      int input_height,
      std::string model_data,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(std::vector<double>)> callback);

  base::MemoryMappedFile visual_tflite_model_;
  base::WeakPtrFactory<Scorer> weak_ptr_factory_{this};

 private:
  friend class PhishingScorerTest;
};

// A small wrapper around a Scorer that allows callers to observe for changes in
// the model.
class ScorerStorage {
 public:
  static ScorerStorage* GetInstance();

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnScorerChanged() = 0;
  };

  ScorerStorage();
  ~ScorerStorage();
  ScorerStorage(const ScorerStorage&) = delete;
  ScorerStorage& operator=(const ScorerStorage&) = delete;

  void SetScorer(std::unique_ptr<Scorer> scorer);
  Scorer* GetScorer() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  std::unique_ptr<Scorer> scorer_;
  base::ObserverList<Observer> observers_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_H_
