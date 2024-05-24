// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This abstract class loads a client-side model and lets you compute a phishing
// score for a set of previously extracted features.  The phishing score
// corresponds to the probability that the features are indicative of a phishing
// site.
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
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
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
  SCORER_FAIL_FLATBUFFER_BAD_INDICES_OR_FIELDS = 10,  // Not used anymore
  SCORER_FAIL_FLATBUFFER_INVALID_IMAGE_EMBEDDING_TFLITE_MODEL = 11,
  SCORER_STATUS_MAX  // Always add new values before this one.
};

// Scorer methods are virtual to simplify mocking of this class,
// and to allow inheritance.
class Scorer {
 public:
  ~Scorer();
  // Most clients should use the factory method.  This constructor is public
  // to allow for mock implementations.
  Scorer();

  // Factory method which creates a new Scorer object by parsing the given
  // flatbuffer or tflite model. If parsing fails this method returns NULL.
  // Use this only if region is valid.
  static std::unique_ptr<Scorer> Create(base::ReadOnlySharedMemoryRegion region,
                                        base::File visual_tflite_model);

  static std::unique_ptr<Scorer> CreateScorerWithImageEmbeddingModel(
      base::ReadOnlySharedMemoryRegion region,
      base::File visual_tflite_model,
      base::File image_embedding_model);

  void AttachImageEmbeddingModel(base::File image_embedding_model);

  // This method computes the probability that the given features are indicative
  // of phishing.  It returns a score value that falls in the range [0.0,1.0]
  // (range is inclusive on both ends).
  double ComputeScore(const FeatureMap& features) const;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // This method applies the TfLite visual model to the given bitmap for image
  // classification. It asynchronously returns the list of scores for each
  // category, in the same order as `tflite_thresholds()`.
  void ApplyVisualTfLiteModel(
      const SkBitmap& bitmap,
      base::OnceCallback<void(std::vector<double>)> callback) const;

  // This method applies the TfLite visual model to the given bitmap for
  // image embedding. It asynchronously returns an ImageFeatureEmbedding object
  // which contains a vector of floats which is the feature vector result from
  // the Image Embedder process.
  void ApplyVisualTfLiteModelImageEmbedding(
      const SkBitmap& bitmap,
      base::OnceCallback<void(ImageFeatureEmbedding)> callback) const;
#endif

  // Returns the version number of the loaded client model.
  int model_version() const;

  int dom_model_version() const;

  bool HasVisualTfLiteModel() const;

  // -- Accessors used by the page feature extractor ---------------------------

  // Returns a callback to find if a page word is in the model.
  base::RepeatingCallback<bool(uint32_t)> find_page_word_callback() const;

  // Returns a callback to find if a page term is in the model.
  base::RepeatingCallback<bool(const std::string&)> find_page_term_callback()
      const;

  // Return the maximum number of words per term for the loaded model.
  size_t max_words_per_term() const;

  // Returns the murmurhash3 seed for the loaded model.
  uint32_t murmurhash3_seed() const;

  // Return the maximum number of unique shingle hashes per page.
  size_t max_shingles_per_page() const;

  // Return the number of words in a shingle.
  size_t shingle_size() const;

  // Returns the threshold probability above which we send a CSD ping.
  float threshold_probability() const;

  // Returns the version of the visual TFLite model.
  int tflite_model_version() const;

  // Returns the image embedding model version.
  int image_embedding_tflite_model_version() const;

  // Returns the thresholds configured for the visual TFLite model categories.
  const google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>&
  tflite_thresholds() const;

  // Disable copy and move.
  Scorer(const Scorer&) = delete;
  Scorer& operator=(const Scorer&) = delete;

 private:
  friend class PhishingScorerTest;

  bool has_page_term(const std::string& str) const;
  bool has_page_word(uint32_t page_word_hash) const;

  double ComputeRuleScore(const flat::ClientSideModel_::Rule* rule,
                          const FeatureMap& features) const;

  // Helper function which converts log odds to a probability in the range
  // [0.0,1.0].
  double LogOdds2Prob(const double log_odds) const;

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

  // Apply the TfLite model to the bitmap. The ImageFeatureEmbedding object is
  // returned by ruinning the `callback` provided by `callback_task_runner`.
  // This is expected to be run on a helper thread.
  static void ApplyImageEmbeddingTfLiteModelHelper(
      const SkBitmap& bitmap,
      int input_width,
      int input_height,
      const std::string& model_data,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(ImageFeatureEmbedding)> callback);

  // Unowned. Points within flatbuffer_mapping_ and should not be free()d.
  // It remains valid till flatbuffer_mapping_ is valid and should be reassigned
  // if the mapping is updated.
  raw_ptr<const flat::ClientSideModel> flatbuffer_model_;
  base::ReadOnlySharedMemoryMapping flatbuffer_mapping_;
  google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>
      thresholds_;
  base::MemoryMappedFile image_embedding_model_;
  base::MemoryMappedFile visual_tflite_model_;
  base::WeakPtrFactory<Scorer> weak_ptr_factory_{this};
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
  // We will clear the scorer in situations where the OptimizationGuide server
  // provides a null model to replace a bad model on disk.
  void ClearScorer();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  std::unique_ptr<Scorer> scorer_;
  base::ObserverList<Observer> observers_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_H_
