// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection//core/language_detection_model.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/metrics_hashes.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/core/language_detection_resolver.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/translate/core/common/translate_constants.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

namespace {

constexpr char kTFLiteModelVersion[] = "TFLite_v1";

// Util class for recording the result of loading the detection model. The
// result is recorded when it goes out of scope and its destructor is called.
class ScopedLanguageDetectionModelStateRecorder {
 public:
  explicit ScopedLanguageDetectionModelStateRecorder(
      language_detection::LanguageDetectionModelState state)
      : state_(state) {}
  ~ScopedLanguageDetectionModelStateRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "LanguageDetection.TFLiteModel.LanguageDetectionModelState", state_);
  }

  void set_state(language_detection::LanguageDetectionModelState state) {
    state_ = state;
  }

 private:
  language_detection::LanguageDetectionModelState state_;
};

}  // namespace

namespace language_detection {

#if !BUILDFLAG(IS_WIN)
BASE_FEATURE(kMmapLanguageDetectionModel,
             "MmapLanguageDetectionModel",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

Prediction TopPrediction(const std::vector<Prediction>& predictions) {
  auto elem = std::max_element(predictions.begin(), predictions.end());
  CHECK(elem != predictions.end());
  return *elem;
}

LanguageDetectionModel::LanguageDetectionModel()
    : num_threads_(
          optimization_guide::features::OverrideNumThreadsForOptTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION)
              .value_or(-1)) {}

LanguageDetectionModel::~LanguageDetectionModel() = default;

std::vector<Prediction> LanguageDetectionModel::Predict(
    const std::u16string& contents,
    bool truncate) const {
  TRACE_EVENT("browser", "LanguageDetectionModel::DetectTopLanguage");
  base::ElapsedTimer timer;

  CHECK(IsAvailable());

  std::string utf8_contents;
  size_t convert_length =
      truncate ? std::min(kModelTruncationLength, contents.length())
               : contents.length();

  base::UTF16ToUTF8(contents.data(), convert_length, &utf8_contents);

  // TFLite expects all strings to be aligned to 4 bytes.
  constexpr size_t kAlignTo = sizeof(int32_t);
  if (utf8_contents.size() % kAlignTo != 0) {
    // Pad the input string to be aligned for TFLite
    utf8_contents +=
        std::string(kAlignTo - utf8_contents.size() % kAlignTo, ' ');
  }

  auto status_or_categories =
      lang_detection_model_->ClassifyText(utf8_contents);
  base::UmaHistogramTimes("LanguageDetection.TFLiteModel.ClassifyText.Duration",
                          timer.Elapsed());
  base::UmaHistogramCounts1M("LanguageDetection.TFLiteModel.ClassifyText.Size",
                             utf8_contents.size());
  base::UmaHistogramCounts1M(
      "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
      contents.size());
  bool detected =
      status_or_categories.ok() && !status_or_categories.value().empty();
  base::UmaHistogramBoolean(
      "LanguageDetection.TFLiteModel.ClassifyText.Detected", detected);
  if (!detected) {
    return {Prediction(translate::kUnknownLanguageCode, 0.0)};
  }
  std::vector<Prediction> predictions;
  predictions.reserve(status_or_categories.value().size());
  for (const auto& category : status_or_categories.value()) {
    predictions.emplace_back(category.class_name, category.score);
  }
  return predictions;
}

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  ScopedLanguageDetectionModelStateRecorder recorder(
      LanguageDetectionModelState::kModelFileInvalid);

  if (!model_file.IsValid()) {
    return;
  }

  recorder.set_state(LanguageDetectionModelState::kModelFileValid);

  tflite::task::text::NLClassifierOptions options;
  options.set_input_tensor_index(0);
  options.set_output_score_tensor_index(0);
  options.set_output_label_tensor_index(2);

  options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(num_threads_);

  base::ElapsedTimer timer;
// Windows doesn't support using mmap for the language detection model.
#if !BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(kMmapLanguageDetectionModel)) {
    options.mutable_base_options()
        ->mutable_model_file()
        ->mutable_file_descriptor_meta()
        ->set_fd(model_file.GetPlatformFile());
  } else
#endif
  {
    std::string file_content(model_file.GetLength(), '\0');
    if (!model_file.ReadAndCheck(0,
                                 base::as_writable_byte_span(file_content))) {
      return;
    }
    *options.mutable_base_options()
         ->mutable_model_file()
         ->mutable_file_content() = std::move(file_content);
  }

  auto statusor_classifier =
      tflite::task::text::nlclassifier::NLClassifier::CreateFromOptions(
          options, CreateLangIdResolver());
  if (!statusor_classifier.ok()) {
    LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.InvalidModelFile",
                            true);
    return;
  }
  base::UmaHistogramTimes("LanguageDetection.TFLiteModel.Create.Duration",
                          timer.Elapsed());

  recorder.set_state(LanguageDetectionModelState::kModelAvailable);

  lang_detection_model_ = std::move(*statusor_classifier);
}

bool LanguageDetectionModel::IsAvailable() const {
  return lang_detection_model_ != nullptr;
}

std::string LanguageDetectionModel::GetModelVersion() const {
  // TODO(crbug.com/40748826): Return the model version provided
  // by the model itself.
  return kTFLiteModelVersion;
}

}  // namespace language_detection
