// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_

#include <string>

#include "base/feature_list.h"
#include "base/files/file.h"
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

#if !BUILDFLAG(IS_WIN)
// Controls whether mmap is used to load the language detection model.
BASE_DECLARE_FEATURE(kMmapLanguageDetectionModel);
#endif

struct Prediction {
  Prediction(const std::string& language, float score)
      : language(language), score(score) {}
  Prediction() = delete;
  std::string language;
  float score;

  bool operator<(const Prediction& other) const { return score < other.score; }
};

// Returns the prediction with the highest score.
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
class LanguageDetectionModel {
 public:
  LanguageDetectionModel();
  ~LanguageDetectionModel();

  // Runs the TFLIte language detection model on the string. This will only look
  // at the first 128 unicode characters of the string. Return a vector of
  // scored language predictions. If `truncate` is `true`, this will truncate
  // the string before passing to the TFLite model. Even though the model only
  // considers a prefix of the input, the runtime is proportional to the total
  // length of the input.
  // TODO(https://crbug.com/352636753): Remove the option and always truncate.
  std::vector<Prediction> Predict(const std::u16string& contents,
                                  bool truncate = true) const;

  // Updates the language detection model for use by memory-mapping
  // |model_file| used to detect the language of the page.
  void UpdateWithFile(base::File model_file);

  // Returns whether |this| is initialized and is available to handle requests
  // to determine the language of the page.
  bool IsAvailable() const;

  std::string GetModelVersion() const;

 private:
  // The tflite classifier that can determine the language of text.
  std::unique_ptr<tflite::task::text::nlclassifier::NLClassifier>
      lang_detection_model_;

  // The number of threads to use for model inference. -1 tells TFLite to use
  // its internal default logic.
  const int num_threads_ = -1;
};

}  // namespace language_detection
#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_MODEL_H_
