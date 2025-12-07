// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace tflite::task::text::nlclassifier {
class NLClassifier;
}  // namespace tflite::task::text::nlclassifier

namespace language_detection {

// Even though the model only looks at the first 128 characters of the string,
// calls to ClassifyText have a run-time proportional to the size of the
// input. So we expect better performance if we truncate the string.
// We use 256 to keep in line with the existing code.
// TODO(https://crbug.com/354070625): Figure out if we can drop this to 128.
inline constexpr size_t kModelTruncationLength = 256;

struct Prediction {
  Prediction(const std::string& language, float score)
      : language(language), score(score) {}
  Prediction() = delete;
  std::string language;
  float score;

  bool operator<(const Prediction& other) const { return score < other.score; }
};

// Returns the prediction with the highest score.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
Prediction TopPrediction(const std::vector<Prediction>& predictions);

// The state of the language detection model file needed for determining
// the language of the page.
//
// Keep in sync with LanguageDetectionModelState in enums.xml.
enum class LanguageDetectionModelState {
  // The language model state is not known.
  kUnknown,
  // The provided model file was not valid.
  kModelFileInvalid,
  // The language model's `base::File` is valid.
  kModelFileValid,
  // The language model is available for use with TFLite.
  kModelAvailable,

  // New values above this line.
  kMaxValue = kModelAvailable,
};

// A language detection model that will use a TFLite model to determine the
// language of a string.
// Each instance of this should only be used from a single thread.
class COMPONENT_EXPORT(LANGUAGE_DETECTION) LanguageDetectionModel {
 public:
  using ModelLoadedCallback = base::OnceCallback<void(LanguageDetectionModel&)>;

  LanguageDetectionModel();
  ~LanguageDetectionModel();

  // Runs the TFLIte language detection model on the string. This will only look
  // at the first 128 unicode characters of the string. Return a vector of
  // scored language predictions. If `truncate` is `true`, this will truncate
  // the string before passing to the TFLite model. Even though the model only
  // considers a prefix of the input, the runtime is proportional to the total
  // length of the input.
  std::vector<Prediction> Predict(std::u16string_view contents) const;

  // Runs the TFLIte language detection model on the whole string. This will
  // scan over the content with the 128 character window.
  // Return a vector of scored language predictions. The predictions are the
  // mean value of the predictions on each window.
  std::vector<Prediction> PredictWithScan(std::u16string_view contents) const;

  // Runs the TFLIte language detection model on no more than three samples of
  // the string. If the contents is less than 768 characters, the function will
  // decide the language by running the model over the first 128 characters.
  // Otherwise, the first, last and the middle 256-character text piece will be
  // sampled and the return value will be the prediction with the highest
  // confidence for the three samples.
  Prediction PredictTopLanguageWithSamples(std::u16string_view contents) const;

  // Updates the language detection model for use by memory-mapping
  // |model_file| used to detect the language of the page.
  //
  // This method is blocking and should only be called in context
  // where it is fine to block the current thread. If you cannot
  // block, use UpdateWithFileAsync(...) instead.
  void UpdateWithFile(base::File model_file);

  // Updates the language detection model for use by memory-mapping
  // |model_file| used to detect the language of the page. Performs
  // the operation on a background sequence and call |callback| on
  // completion
  void UpdateWithFileAsync(base::File model_file, base::OnceClosure callback);

  // Returns whether |this| is initialized and is available to handle requests
  // to determine the language of the page.
  bool IsAvailable() const;

  // Returns the size of the loaded model in bytes. If the model is not yet
  // available, the method will return 0.
  int64_t GetModelSize() const;

  void AddOnModelLoadedCallback(ModelLoadedCallback callback);

  std::string GetModelVersion() const;

  // Detach the instance from the bound sequence. Must only be used if the
  // object is created on a sequence and then moved on another sequence to
  // live.
  void DetachFromSequence() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  // The number of characters to sample and provide as a buffer to the model
  // in PredictTopLanguageWithSamples.
  static constexpr size_t kTextSampleLength = 256;

  // The number of samples of |kTextSampleLength| to evaluate the model
  // in PredictTopLanguageWithSamples.
  static constexpr int kNumTextSamples = 3;

  // The maximum window size the model runs over when predicting the language.
  static constexpr size_t kScanWindowSize = 128;

 private:
  // An owned NLClassifier.
  using OwnedNLClassifier =
      std::unique_ptr<tflite::task::text::nlclassifier::NLClassifier>;
  using ModelAndSize = std::pair<OwnedNLClassifier, int64_t>;

  // Loads model from |model_file| using |num_threads|. This can be called on
  // any thread.
  static std::optional<LanguageDetectionModel::ModelAndSize> LoadModelFromFile(
      base::File model_file,
      int num_threads);

  void NotifyModelLoaded();

  // Execute the model on the provided |sampled_str| and return the top
  // language and the models score/confidence in that prediction.
  Prediction DetectTopLanguage(std::u16string_view sampled_str) const;

  // Updates the model if the not unset.
  void SetModel(std::optional<ModelAndSize> model_and_size);

  SEQUENCE_CHECKER(sequence_checker_);

  // The tflite classifier that can determine the language of text.
  OwnedNLClassifier lang_detection_model_;

  // The number of threads to use for model inference. -1 tells TFLite to use
  // its internal default logic.
  const int num_threads_ = -1;

  static constexpr int kMaxPendingCallbacksCount = 100;
  // Pending callbacks for waiting the model to be available.
  std::vector<ModelLoadedCallback> model_loaded_callbacks_;

  // Records whether a file has been updated to the model.
  bool loaded_ = false;

  // Records the size of the model file loaded. The value is only valid when
  // loaded_ is True.
  int64_t model_file_size_ = 0;

  // Used to load the data on a background sequence (see UpdateWithFileAsync).
  base::WeakPtrFactory<LanguageDetectionModel> weak_factory_{this};
};

}  // namespace language_detection
#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_
