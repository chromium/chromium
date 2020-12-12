// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_

#include <string>
#include "base/files/memory_mapped_file.h"

namespace translate {

// The state of the language detection model file needed for determining
// the language of the page.
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
  // Returns the contents language results in |predicted_language| and
  // |is_prediction_reliable|.
  std::string DeterminePageLanguage(const std::string& code,
                                    const std::string& html_lang,
                                    const base::string16& contents,
                                    std::string* predicted_language,
                                    bool* is_prediction_reliable) const;

 private:
  // A memory-mapped file that contains the TFLite model used for
  // determining the language of a page. This must be valid in order
  // to evaluate the model owned by |this|.
  //
  // TODO(crbug.com/1151413): Add the tflite language detection model.
  base::MemoryMappedFile model_fb_;
};

}  // namespace translate
#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_MODEL_H_
