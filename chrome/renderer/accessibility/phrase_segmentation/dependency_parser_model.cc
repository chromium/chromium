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
#include "third_party/tensorflow-text/src/tensorflow_text/core/kernels/mst_solver.h"
#include "third_party/tensorflow_models/src/research/seq_flow_lite/tflite_ops/quantization_util.h"
#include "third_party/tflite/src/tensorflow/lite/string_util.h"
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

std::vector<unsigned int> DependencyParserModel::GetDependencyHeads(
    std::vector<std::string> input) {
  DCHECK(IsAvailable());
  base::ElapsedTimer timer;

  // Perform the following operations to identify the dependency heads for each
  // token:
  // 1. Processes the input (tokenized string, length N) using the TFLite
  // model to generate a dependency probability matrix (NxN).
  // 2. Utilizes a Minimum Spanning Tree (MST) algorithm to identify the
  // dependency head for each token.
  auto* interpreter = dependency_parser_model_->interpreter();
  interpreter->ResizeInputTensor(0, {1, static_cast<int>(input.size())});
  TfLiteTensor* input_tensor = interpreter->input_tensor(0);
  tflite::DynamicBuffer input_buffer;

  for (absl::string_view token : input) {
    tflite::StringRef string_ref;
    string_ref.str = token.data();
    string_ref.len = token.size();
    input_buffer.AddString(string_ref);
  }
  // Populate tensors.
  input_buffer.WriteToTensor(input_tensor, /*new_shape=*/nullptr);
  interpreter->AllocateTensors();

  interpreter->Invoke();
  base::UmaHistogramTimes(
      "Accessibility.DependencyParserModel.Inference.Duration",
      timer.Elapsed());
  base::UmaHistogramCounts1M(
      "Accessibility.DependencyParserModel.Inference.LengthInTokens",
      input.size());

  const TfLiteTensor* output_tensor = interpreter->output_tensor(0);
  if (output_tensor == nullptr) {
    DLOG(ERROR) << "Error: output tensor is null.";
    return std::vector<unsigned int>();
  }
  size_t size = output_tensor->dims->data[0];
  base::UmaHistogramBoolean(
      "Accessibility.DependencyParserModel.Inference.Succeed",
      size == input.size());
  if (size != input.size()) {
    DLOG(ERROR) << "Error: output tensor size does not match input size.";
    return std::vector<unsigned int>();
  }

  std::vector<std::vector<float>> dependency_graph;
  dependency_graph.reserve(size);
  for (size_t j = 0; j < size; j++) {
    std::vector<float> dependency_graph_inner;
    dependency_graph_inner.reserve(size);
    for (size_t i = 0; i < size; i++) {
      dependency_graph_inner.push_back(
          seq_flow_lite::PodDequantize<uint8_t>(*output_tensor, j * size + i));
    }
    dependency_graph.emplace_back(dependency_graph_inner);
  }

  std::vector<unsigned int> dependency_heads =
      SolveDependencies(dependency_graph);
  return dependency_heads;
}

std::vector<unsigned int> DependencyParserModel::SolveDependencies(
    base::span<const std::vector<float>> input) {
  tensorflow::text::MstSolver<unsigned int, float> solver;
  int size = input.size();
  if (!solver.Init(/*forest=*/false, size).ok()) {
    return {};
  }

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      if (i == j) {
        solver.AddRoot(i, input[i][j]);
      } else {
        solver.AddArc(j, i, input[i][j]);
      }
    }
  }

  std::vector<unsigned int> heads;
  heads.resize(size);
  if (!solver.Solve(&heads).ok()) {
    return {};
  }
  return heads;
}
