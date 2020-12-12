// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros_local.h"
#include "components/translate/core/common/translate_constants.h"

namespace translate {

LanguageDetectionModel::LanguageDetectionModel() = default;

LanguageDetectionModel::~LanguageDetectionModel() = default;

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  // TODO(crbug.com/1157661): Update to be full histograms.
  if (!model_file.IsValid()) {
    LOCAL_HISTOGRAM_ENUMERATION(
        "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
        LanguageDetectionModelState::kModelFileInvalid);
    return;
  }

  if (!model_fb_.Initialize(std::move(model_file))) {
    LOCAL_HISTOGRAM_ENUMERATION(
        "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
        LanguageDetectionModelState::kModelFileInvalid);
    return;
  }

  LOCAL_HISTOGRAM_ENUMERATION(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileValidAndMemoryMapped);

  // TODO(crbug.com/1151413): Initialize tflite classifier with the provided
  // language detection model in |model_fb_|.
}

bool LanguageDetectionModel::IsAvailable() const {
  return model_fb_.IsValid();
}

std::string LanguageDetectionModel::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const base::string16& contents,
    std::string* predicted_language,
    bool* is_prediction_reliable) const {
  DCHECK(IsAvailable());
  // TODO(crbug.com/1151413): Execute the tflite language detection
  // model and finalize the result with the language detection utilty.

  LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLite.DidDetectPageLanguage",
                          true);
  *is_prediction_reliable = false;
  *predicted_language = translate::kUnknownLanguageCode;
  return translate::kUnknownLanguageCode;
}

}  // namespace translate