// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_model.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/core/constants.h"
#include "components/language_detection/core/language_detection_resolver.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

namespace language_detection {
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

Prediction TopPrediction(const std::vector<Prediction>& predictions) {
  auto elem = std::max_element(predictions.begin(), predictions.end());
  CHECK(elem != predictions.end());
  return *elem;
}

// static
std::optional<LanguageDetectionModel::ModelAndSize>
LanguageDetectionModel::LoadModelFromFile(base::File model_file,
                                          int num_threads) {
  ScopedLanguageDetectionModelStateRecorder recorder(
      LanguageDetectionModelState::kModelFileInvalid);

  if (!model_file.IsValid()) {
    return std::nullopt;
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
      ->set_num_threads(num_threads);

  base::ElapsedTimer timer;
// Windows doesn't support using mmap for the language detection model.
#if !BUILDFLAG(IS_WIN)
  options.mutable_base_options()
      ->mutable_model_file()
      ->mutable_file_descriptor_meta()
      ->set_fd(model_file.GetPlatformFile());
#else
  {
    std::string file_content(model_file.GetLength(), '\0');
    if (!model_file.ReadAndCheck(0,
                                 base::as_writable_byte_span(file_content))) {
      return std::nullopt;
    }
    *options.mutable_base_options()
         ->mutable_model_file()
         ->mutable_file_content() = std::move(file_content);
  }
#endif

  auto statusor_classifier =
      tflite::task::text::nlclassifier::NLClassifier::CreateFromOptions(
          options, CreateLangIdResolver());
  if (!statusor_classifier.ok()) {
    LOCAL_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.InvalidModelFile",
                            true);
    return std::nullopt;
  }
  base::UmaHistogramTimes("LanguageDetection.TFLiteModel.Create.Duration",
                          timer.Elapsed());

  recorder.set_state(LanguageDetectionModelState::kModelAvailable);

  return std::make_pair(std::move(statusor_classifier).value(),
                        model_file.GetLength());
}

LanguageDetectionModel::LanguageDetectionModel()
    : num_threads_(
          optimization_guide::features::OverrideNumThreadsForOptTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION)
              .value_or(-1)) {}

LanguageDetectionModel::~LanguageDetectionModel() = default;

std::vector<Prediction> LanguageDetectionModel::Predict(
    std::u16string_view contents) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("browser", "LanguageDetectionModel::DetectTopLanguage");
  base::ElapsedTimer timer;

  CHECK(IsAvailable());

  size_t convert_length = std::min(kModelTruncationLength, contents.length());
  std::string utf8_contents;
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
    return {Prediction(kUnknownLanguageCode, 0.0)};
  }
  std::vector<Prediction> predictions;
  predictions.reserve(status_or_categories.value().size());
  for (const auto& category : status_or_categories.value()) {
    predictions.emplace_back(category.class_name, category.score);
  }
  return predictions;
}

std::vector<Prediction> LanguageDetectionModel::PredictWithScan(
    std::u16string_view contents) const {
  std::map<std::string, double> score_by_language;
  size_t pos = 0;
  size_t count = 0;
  while (pos < contents.length()) {
    std::u16string_view substring = contents.substr(pos, kScanWindowSize);
    pos += kScanWindowSize;
    count++;
    auto predictions = Predict(substring);
    for (const auto& prediction : predictions) {
      score_by_language[prediction.language] += prediction.score;
    }
  }
  std::vector<Prediction> predictions;
  predictions.reserve(score_by_language.size());
  for (const auto& it : score_by_language) {
    predictions.emplace_back(it.first, it.second / count);
  }
  if (predictions.empty()) {
    return {Prediction(kUnknownLanguageCode, 0.0)};
  }
  return predictions;
}

Prediction LanguageDetectionModel::DetectTopLanguage(
    std::u16string_view sampled_str) const {
  TRACE_EVENT("browser", "LanguageDetectionModel::DetectTopLanguage");

  std::vector<Prediction> predictions = Predict(sampled_str);
  Prediction top_prediction = TopPrediction(predictions);
  base::UmaHistogramSparse(
      "LanguageDetection.TFLiteModel.ClassifyText.HighestConfidenceLanguage",
      base::HashMetricName(top_prediction.language));
  return top_prediction;
}

Prediction LanguageDetectionModel::PredictTopLanguageWithSamples(
    std::u16string_view contents) const {
  std::vector<Prediction> model_predictions;
  // First evaluate the model on the entire contents based on the model's
  // implementation, for v1 it is the first 128 tokens that are unicode
  // "letters". We do not need to have the model's length in sync with
  // the sampling logic for v1 as 128 tokens is unlikely to be changed.
  model_predictions.emplace_back(DetectTopLanguage(contents));

  if (contents.length() > kNumTextSamples * kTextSampleLength) {
    // Strings with UTF-8 have different widths so substr should be performed on
    // the UTF16 strings to ensure alignment and then convert down to UTF-8
    // strings for model evaluation.
    std::u16string_view sampled_str = contents.substr(
        contents.length() - kTextSampleLength, kTextSampleLength);
    // Evaluate on the last |kTextSampleLength| characters.
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));

    // Sample and evaluate on the middle |kTextSampleLength| characters.
    sampled_str = contents.substr(contents.length() / 2, kTextSampleLength);
    model_predictions.emplace_back(DetectTopLanguage(sampled_str));
  }
  return *std::max_element(model_predictions.begin(), model_predictions.end());
}

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetModel(LoadModelFromFile(std::move(model_file), num_threads_));
}

void LanguageDetectionModel::UpdateWithFileAsync(base::File model_file,
                                                 base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadModelFromFile, std::move(model_file), num_threads_),
      base::BindOnce(&LanguageDetectionModel::SetModel,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

bool LanguageDetectionModel::IsAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lang_detection_model_ != nullptr;
}

int64_t LanguageDetectionModel::GetModelSize() const {
  if (!IsAvailable()) {
    return 0;
  }
  return model_file_size_;
}

std::string LanguageDetectionModel::GetModelVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40748826): Return the model version provided
  // by the model itself.
  return kTFLiteModelVersion;
}

void LanguageDetectionModel::SetModel(
    std::optional<ModelAndSize> model_and_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (model_and_size.has_value()) {
    lang_detection_model_ = std::move(model_and_size.value().first);
    model_file_size_ = model_and_size.value().second;
  } else {
    model_file_size_ = 0;
  }
  NotifyModelLoaded();
}

void LanguageDetectionModel::AddOnModelLoadedCallback(
    ModelLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loaded_ || model_loaded_callbacks_.size() >= kMaxPendingCallbacksCount) {
    std::move(callback).Run(*this);
  } else {
    model_loaded_callbacks_.emplace_back(std::move(callback));
  }
}

void LanguageDetectionModel::NotifyModelLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto&& callback : model_loaded_callbacks_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](ModelLoadedCallback callback,
                          base::WeakPtr<LanguageDetectionModel> model) {
                         if (model) {
                           std::move(callback).Run(*model);
                         }
                       },
                       std::move(callback), weak_factory_.GetWeakPtr()));
  }
  loaded_ = true;
  model_loaded_callbacks_.clear();
}

}  // namespace language_detection
