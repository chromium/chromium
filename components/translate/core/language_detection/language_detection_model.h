// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "build/build_config.h"
#include "components/language_detection/core/language_detection_model.h"
#include "partition_alloc/pointers/raw_ref.h"

namespace translate {
BASE_DECLARE_FEATURE(kTruncateLanguageDetectionSample);

// A language detection model that will use a TFLite model to determine the
// language of the content of the web page.
class LanguageDetectionModel {
 public:
  explicit LanguageDetectionModel(
      language_detection::LanguageDetectionModel* tflite_model_);
  ~LanguageDetectionModel();

#if !BUILDFLAG(IS_IOS)
  // Updates the language detection model for use by memory-mapping
  // |model_file| used to detect the language of the page.
  void UpdateWithFile(base::File model_file);
#endif

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

  language_detection::Prediction DetectLanguage(
      const std::u16string& contents) const;

  std::string GetModelVersion() const;

 private:
  // Execute the model on the provided |sampled_str| and return the top language
  // and the models score/confidence in that prediction.
  std::pair<std::string, float> DetectTopLanguage(
      const std::u16string& sampled_str) const;

  // The tflite classifier that can determine the language of text.
  const raw_ptr<language_detection::LanguageDetectionModel> tflite_model_;
};

}  // namespace translate
#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
