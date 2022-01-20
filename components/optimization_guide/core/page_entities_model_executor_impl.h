// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_IMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/page_entities_model_executor.h"

namespace optimization_guide {

class EntityAnnotatorNativeLibrary;
class OptimizationGuideModelProvider;

// An object used to hold an entity annotator on a background thread.
class EntityAnnotatorHolder {
 public:
  EntityAnnotatorHolder(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner);
  ~EntityAnnotatorHolder();

  // Initializes the native library on a background thread. Will invoke
  // |init_callback| on |reply_task_runner_| with the max version supported for
  // the entity annotator on success. Otherwise, -1.
  void InitializeEntityAnnotatorNativeLibraryOnBackgroundThread(
      base::OnceCallback<void(int32_t)> init_callback);

  // Creates an entity annotator on the background thread and sets it to
  // |entity_annotator_|. Should be invoked on |background_task_runner_|.
  void CreateAndSetEntityAnnotatorOnBackgroundThread(
      const ModelInfo& model_info);

  // Requests for |entity_annotator_| to execute its model for |text| and map
  // the entities back to their metadata. Should be invoked on
  // |background_task_runner_|.
  using PageEntitiesMetadataModelExecutedCallback = base::OnceCallback<void(
      const absl::optional<std::vector<ScoredEntityMetadata>>&)>;
  void AnnotateEntitiesMetadataModelOnBackgroundThread(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback);

  // Returns entity metadata from |entity_annotator_| for |entity_id|.
  // Should be invoked on |background_task_runner_|.
  void GetMetadataForEntityIdOnBackgroundThread(
      const std::string& entity_id,
      PageEntitiesModelExecutor::
          PageEntitiesModelEntityMetadataRetrievedCallback callback);

  // Gets the weak ptr to |this| on the background thread.
  base::WeakPtr<EntityAnnotatorHolder> GetBackgroundWeakPtr();

 private:
  void ResetEntityAnnotator();

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;

  std::unique_ptr<EntityAnnotatorNativeLibrary>
      entity_annotator_native_library_;
  void* entity_annotator_ = nullptr;

  base::WeakPtrFactory<EntityAnnotatorHolder> background_weak_ptr_factory_{
      this};
};

// Manages the loading and execution of the page entities model.
class PageEntitiesModelExecutorImpl : public OptimizationTargetModelObserver,
                                      public PageEntitiesModelExecutor {
 public:
  PageEntitiesModelExecutorImpl(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  ~PageEntitiesModelExecutorImpl() override;
  PageEntitiesModelExecutorImpl(const PageEntitiesModelExecutorImpl&) = delete;
  PageEntitiesModelExecutorImpl& operator=(
      const PageEntitiesModelExecutorImpl&) = delete;

  // PageEntitiesModelExecutor:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      PageEntitiesModelEntityMetadataRetrievedCallback callback) override;
  void HumanReadableExecuteModelWithInput(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback) override;

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override;

 private:
  // Invoked on the UI thread when entity annotator library has been
  // initialized.
  void OnEntityAnnotatorLibraryInitialized(
      OptimizationGuideModelProvider* model_provider,
      int32_t max_model_format_feature_flag);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The holder used to hold the annotator used to annotate entities.
  std::unique_ptr<EntityAnnotatorHolder> entity_annotator_holder_;

  base::WeakPtrFactory<PageEntitiesModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_IMPL_H_
