// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_op_resolver.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace {

// Util class for recording the result of loading the dependency parser model.
// The result is recorded when it goes out of scope and its destructor is
// called.
class ScopedDependencyParserModelStateRecorder {
 public:
  explicit ScopedDependencyParserModelStateRecorder(
      DependencyParserModelState state)
      : state_(state) {}

  ScopedDependencyParserModelStateRecorder(
      const ScopedDependencyParserModelStateRecorder&) = delete;
  ScopedDependencyParserModelStateRecorder& operator=(
      const ScopedDependencyParserModelStateRecorder&) = delete;

  ~ScopedDependencyParserModelStateRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "Accessibility.DependencyParserModel.DependencyParserModelState",
        state_);
  }

  void set_state(DependencyParserModelState state) { state_ = state; }

 private:
  DependencyParserModelState state_;
};

}  // namespace

DependencyParserModel::DependencyParserModel()
    : num_threads_(optimization_guide::features::OverrideNumThreadsForOptTarget(
                       optimization_guide::proto::
                           OPTIMIZATION_TARGET_PHRASE_SEGMENTATION)
                       .value_or(-1)) {}

DependencyParserModel::~DependencyParserModel() = default;

void DependencyParserModel::UpdateWithFile(base::File model_file) {
  ScopedDependencyParserModelStateRecorder recorder(
      DependencyParserModelState::kModelFileInvalid);

  if (!model_file.IsValid()) {
    return;
  }

  base::ElapsedTimer timer;
  std::string file_content(model_file.GetLength(), '\0');
  if (!model_file.ReadAndCheck(0, base::as_writable_byte_span(file_content))) {
    return;
  }

  auto tflite_engine = std::make_unique<tflite::task::core::TfLiteEngine>(
      std::make_unique<DependencyParserOpResolver>());
  absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
      reinterpret_cast<const char*>(std::data(file_content)),
      model_file.GetLength());
  if (!model_load_status.ok()) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "Accessibility.DependencyParserModel.InvalidModelFile", true);
    DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
    return;
  }

  recorder.set_state(DependencyParserModelState::kModelFileValid);

  auto compute_settings = tflite::proto::ComputeSettings();
  compute_settings.mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(num_threads_);
  absl::Status interpreter_status =
      tflite_engine->InitInterpreter(compute_settings);
  if (!interpreter_status.ok()) {
    DLOG(ERROR) << "Failed to initialize model interpreter: "
                << interpreter_status.ToString();
    return;
  }
  base::UmaHistogramTimes("Accessibility.DependencyParserModel.Create.Duration",
                          timer.Elapsed());

  recorder.set_state(DependencyParserModelState::kModelAvailable);

  dependency_parser_model_ = std::move(tflite_engine);
}

bool DependencyParserModel::IsAvailable() const {
  return dependency_parser_model_ != nullptr;
}

int64_t DependencyParserModel::GetModelVersion() const {
  // TODO(b/339037155): Return the model version provided
  // by the model itself.
  return 1;
}

std::vector<unsigned int> GetDependencyHeads(std::vector<std::string> input) {
  // TODO(b/339037155): Implement the operations to get the dependency heads for
  // each word in a sentence. This method performs the following operations:
  // 1. Tokenizes the input string.
  // 2. Processes the tokens (N tokens) using the TFLite model to generate a
  // dependency probability matrix (NxN).
  // 3. Utilizes a Minimum Spanning Tree (MST) algorithm to identify the
  // dependency head for each word.
  return std::vector<unsigned int>();
}
