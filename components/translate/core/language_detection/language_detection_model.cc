// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "components/translate/core/common/translate_constants.h"

namespace {

constexpr char kTFLiteModelVersion[] = "TFLite_v1";

// Util class for recording the result of loading the detection model. The
// result is recorded when it goes out of scope and its destructor is called.
class ScopedLanguageDetectionModelStateRecorder {
 public:
  explicit ScopedLanguageDetectionModelStateRecorder(
      translate::LanguageDetectionModelState state)
      : state_(state) {}
  ~ScopedLanguageDetectionModelStateRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "LanguageDetection.TFLiteModel.LanguageDetectionModelState", state_);
  }

  void set_state(translate::LanguageDetectionModelState state) {
    state_ = state;
  }

 private:
  translate::LanguageDetectionModelState state_;
};

}  // namespace

namespace translate {

LanguageDetectionModel::LanguageDetectionModel() = default;

LanguageDetectionModel::~LanguageDetectionModel() = default;

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  ScopedLanguageDetectionModelStateRecorder recorder(
      LanguageDetectionModelState::kModelFileInvalid);

  if (!model_file.IsValid())
    return;

  if (!model_fb_.Initialize(std::move(model_file)))
    return;

  recorder.set_state(
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
    bool* is_prediction_reliable,
    float& prediction_reliability_score) const {
  DCHECK(IsAvailable());
  // TODO(crbug.com/1151413): Execute the tflite language detection
  // model and finalize the result with the language detection utilty.

  LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLite.DidDetectPageLanguage",
                          true);
  *is_prediction_reliable = false;
  *predicted_language = translate::kUnknownLanguageCode;
  prediction_reliability_score = 0.0;
  return translate::kUnknownLanguageCode;
}

std::string LanguageDetectionModel::GetModelVersion() const {
  // TODO(crbug.com/1177992): Return the model version provided
  // by the model itself.
  return kTFLiteModelVersion;
}

}  // namespace translate