// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_

#include <string>

#include "base/files/file.h"

namespace tflite {
namespace task {
namespace text {
namespace nlclassifier {
class NLClassifier;
}
}  // namespace text
}  // namespace task
}  // namespace tflite

namespace translate {

// The state of the language detection model file needed for determining
// the language of the page.
//
// Keep in sync with LanguageDetectionModelState in enums.xml.
enum class LanguageDetectionModelState {
  // The language model state is not known.
  kUnknown,
  // The provided model file was not valid.
  kModelFileInvalid,
  // The language model is memory-mapped and available for
  // use with TFLite.
  kModelFileValidAndMemoryMapped,

  // New values above this line.
  kMaxValue = kModelFileValidAndMemoryMapped,
};

// A language detection model that will use a TFLite model to determine the
// language of the content of the web page.
class LanguageDetectionModel {
 public:
  LanguageDetectionModel();
  ~LanguageDetectionModel();

  // Updates the language detection model for use by memory-mapping
  // |model_file| used to detect the language of the page.
  void UpdateWithFile(base::File model_file);

  // Returns whether |this| is initialized and is available to handle requests
  // to determine the language of the page.
  bool IsAvailable() const;

  // Determines content page language from Content-Language code and contents.
  // Returns the contents language results in |predicted_language|,
  // |is_prediction_reliable|, and |prediction_reliability_score|.
  std::string DeterminePageLanguage(const std::string& code,
                                    const std::string& html_lang,
                                    const std::u16string& contents,
                                    std::string* predicted_language,
                                    bool* is_prediction_reliable,
                                    float& prediction_reliability_score) const;

  std::string GetModelVersion() const;

 private:
  // Execute the model on the provided |sampled_str| and return the top language
  // and the models score/confidence in that prediction.
  std::pair<std::string, float> DetectTopLanguage(
      const std::string& sampled_str) const;

  // The tflite classifier that can determine the language of text.
  std::unique_ptr<tflite::task::text::nlclassifier::NLClassifier>
      lang_detection_model_;

  // The number of threads to use for model inference. -1 tells TFLite to use
  // its internal default logic.
  const int num_threads_ = -1;
};

}  // namespace translate
#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
