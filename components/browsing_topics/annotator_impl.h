// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_ANNOTATOR_IMPL_H_
#define COMPONENTS_BROWSING_TOPICS_ANNOTATOR_IMPL_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browsing_topics/annotator.h"
#include "components/optimization_guide/core/bert_model_handler.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace browsing_topics {

// An implementation of the |Annotator| base class. This Annotator supports
// concurrent batch annotations and manages the lifetimes of all underlying
// components. This class must only be owned and called on the UI thread.
//
// |BatchAnnotate| is the main entry point for callers. The callback given to
// |BatchAnnotate| is forwarded through many subsequent PostTasks until all
// annotations are ready to be returned to the caller.
//
// Life of an Annotation:
// 1. |BatchAnnotate| checks if the override list needs to be loaded. If so, it
// is done on a background thread. After that check and possibly loading the
// list in |OnOverrideListLoadAttemptDone|, |StartBatchAnnotate| is called.
// 2. |StartBatchAnnotate| shares ownership of the |BatchAnnotationCallback|
// among a series of callbacks (using |base::BarrierClosure|), one for each
// input. Ownership of the inputs is moved to the heap where all individual
// model executions can reference their input and set their output.
// 3. |AnnotateSingleInput| runs a single annotation, first checking the
// override list if available. If the input is not covered in the override list,
// the ML model is run on a background thread.
// 4. |PostprocessCategoriesToBatchAnnotationResult| is called to post-process
// the output of the ML model.
// 5. |OnBatchComplete| is called by the barrier closure which passes the
// annotations back to the caller and unloads the model if no other batches are
// in progress.
class AnnotatorImpl : public Annotator,
                      public optimization_guide::BertModelHandler {
 public:
  AnnotatorImpl(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::optional<optimization_guide::proto::Any>& model_metadata);
  ~AnnotatorImpl() override;

  // Annotator:
  void BatchAnnotate(BatchAnnotationCallback callback,
                     const std::vector<std::string>& inputs) override;
  void NotifyWhenModelAvailable(base::OnceClosure callback) override;
  std::optional<optimization_guide::ModelInfo> GetBrowsingTopicsModelInfo()
      const override;

  //////////////////////////////////////////////////////////////////////////////
  // Public methods below here are exposed only for testing.
  //////////////////////////////////////////////////////////////////////////////

  // optimization_guide::BertModelHandler:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // Extracts the scored categories from the output of the model.
  std::optional<std::vector<int32_t>> ExtractCategoriesFromModelOutput(
      const std::vector<tflite::task::core::Category>& model_output) const;

 protected:
  // optimization_guide::BertModelHandler:
  void UnloadModel() override;

 private:
  // Sets the |override_list_| after it was loaded on a background thread and
  // calls |StartBatchAnnotate|.
  void OnOverrideListLoadAttemptDone(
      BatchAnnotationCallback callback,
      const std::vector<std::string>& inputs,
      std::optional<std::unordered_map<std::string, std::vector<int32_t>>>
          override_list);

  // Starts a batch annotation once the override list is loaded, if provided.
  void StartBatchAnnotate(BatchAnnotationCallback callback,
                          const std::vector<std::string>& inputs);

  // Does the required preprocessing on a input domain.
  std::string PreprocessHost(const std::string& host) const;

  // Runs a single input through the ML model, setting the result in
  // |annotation|.
  void AnnotateSingleInput(base::OnceClosure single_input_done_signal,
                           Annotation* annotation);

  // Called when all single inputs have been annotated and the |callback| from
  // the caller can finally be run.
  void OnBatchComplete(
      BatchAnnotationCallback callback,
      std::unique_ptr<std::vector<Annotation>> annotations_ptr);

  // Sets |annotation.topics| from the output of the model, calling
  // |ExtractCategoriesFromModelOutput| in the process.
  void PostprocessCategoriesToBatchAnnotationResult(
      base::OnceClosure single_input_done_signal,
      Annotation* annotation,
      const std::optional<std::vector<tflite::task::core::Category>>& output);

  // Used to read the override list file on a background thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Set whenever a valid override list file is passed along with the model file
  // update. Used on the UI thread.
  std::optional<base::FilePath> override_list_file_path_;

  // Set whenever an override list file is available and the model file is
  // loaded into memory. Reset whenever the model file is unloaded.
  // Used on the UI thread. Lookups in this mapping should have |PreprocessHost|
  // applied first.
  std::optional<std::unordered_map<std::string, std::vector<int32_t>>>
      override_list_;

  // The version of topics model provided by the server in the model metadata
  // which specifies the expected functionality of execution not contained
  // within the model itself (e.g., preprocessing/post processing).
  int version_ = 0;

  // Counts the number of batches that are in progress. This counter is
  // incremented in |StartBatchAnnotate| and decremented in |OnBatchComplete|.
  // When this counter is 0 in |OnBatchComplete|, the model in unloaded from
  // memory.
  size_t in_progess_batches_ = 0;

  // Callbacks that are run when the model is updated with the correct taxonomy
  // version.
  base::OnceClosureList model_available_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AnnotatorImpl> weak_ptr_factory_{this};
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_ANNOTATOR_IMPL_H_
