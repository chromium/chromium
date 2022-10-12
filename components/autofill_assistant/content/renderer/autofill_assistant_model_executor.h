// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor_result.h"
#include "components/autofill_assistant/content/renderer/model_metadata.pb.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/core/base_model_executor_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/text/tokenizers/regex_tokenizer.h"

namespace autofill_assistant {

// Holds and executes the AnnotateDOM model to predict semantic roles based on
// node signals.
class AutofillAssistantModelExecutor
    : public optimization_guide::BaseModelExecutor<
          ModelExecutorResult,
          const blink::AutofillAssistantNodeSignals&> {
 public:
  using ExecutionTask = optimization_guide::GenericModelExecutionTask<
      ModelExecutorResult,
      const blink::AutofillAssistantNodeSignals&>;
  using SparseVector = std::vector<std::pair<std::pair<int, int>, int>>;
  using SparseMap = base::flat_map<std::pair<int, int>, int>;
  using OverridesMap = base::flat_map<SparseVector, std::pair<int, int>>;

  explicit AutofillAssistantModelExecutor(
      absl::optional<OverridesMap> policy = absl::nullopt);
  ~AutofillAssistantModelExecutor() override;

  AutofillAssistantModelExecutor(const AutofillAssistantModelExecutor&) =
      delete;
  void operator=(const AutofillAssistantModelExecutor&) = delete;

  // Initialize the model from the |model_file|. Sets |model_initialized_|.
  bool InitializeModelFromFile(base::File model_file);

  // Execute the model with the given input.
  absl::optional<ModelExecutorResult> ExecuteModelWithInput(
      const blink::AutofillAssistantNodeSignals& node_signals);

 protected:
  // optimization_guide::InferenceDelegate:
  bool Preprocess(
      const std::vector<TfLiteTensor*>& input_tensors,
      const blink::AutofillAssistantNodeSignals& node_signals) override;
  absl::optional<ModelExecutorResult> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

 private:
  // Initialize Tokenizers with |pattern| for splitting and the |vocabulary| in
  // the form of "<word> <index>" per line.
  void InitializeTagsTokenizer(const std::string& pattern,
                               const std::string& vocabulary);
  void InitializeTypesTokenizer(const std::string& pattern,
                                const std::string& vocabulary);
  void InitializeTextTokenizer(const std::string& pattern,
                               const std::string& vocabulary);

  // Build the execution task from the model file.
  void BuildExecutionTask(
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine);

  // Tokenize the |input| and count words into the |output_map|. The same
  // |output_map| should be reused for all relevant inputs for a signal.
  void Tokenize(const std::u16string& input,
                tflite::support::text::tokenizer::RegexTokenizer* tokenizer,
                const int feature_index,
                SparseMap& output_map);

  SparseVector TokenizeSignalsToSparseVector(
      const blink::AutofillAssistantNodeSignals& node_signals);

  // Helper functions for post processing based on |model_metadata_|.
  bool GetIndexOfBestRole(const std::vector<float>& output_role,
                          size_t* index_of_best_role) const;
  bool GetBlockIndex(const std::vector<float>& output_role,
                     size_t index_of_best_role,
                     int* block_index) const;
  bool GetObjective(const std::vector<float>& output_objective,
                    int block_index,
                    int* objective) const;

  // Tokenizer for HTML tag.
  std::unique_ptr<tflite::support::text::tokenizer::RegexTokenizer>
      tags_tokenizer_;
  // Tokenizer for HTML attribute "type".
  std::unique_ptr<tflite::support::text::tokenizer::RegexTokenizer>
      types_tokenizer_;
  // Tokenizer for arbitrary text.
  std::unique_ptr<tflite::support::text::tokenizer::RegexTokenizer>
      text_tokenizer_;

  // The task for this executor.
  std::unique_ptr<ExecutionTask> execution_task_;
  // Model file held in memory by this instance.
  base::MemoryMappedFile model_file_;
  // Model Metadata for handling input/output.
  ModelMetadata model_metadata_;
  // Data regarding business logic for model execution.
  // Set if there is an override for this model execution.
  // Sparse encoding of a feature vector table.
  // The format is: overrides_[vector] = (semantic_role, objective)
  absl::optional<OverridesMap> overrides_;
  absl::optional<std::pair<int, int>> overrides_result_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_H_
