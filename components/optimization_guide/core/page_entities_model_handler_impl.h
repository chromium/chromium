// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/page_entities_model_handler.h"

namespace optimization_guide {

class EntityAnnotatorNativeLibrary;
class OptimizationGuideModelProvider;

// A configuration that manages the necessary feature params required by the
// PageEntitiesModelHandler.
struct PageEntitiesModelHandlerConfig {
  // Whether the page entities model should be reset on shutdown.
  bool should_reset_entity_annotator_on_shutdown = false;

  // Whether the path to the filters should be provided to the page entities
  // model.
  bool should_provide_filter_path = true;

  PageEntitiesModelHandlerConfig();
  PageEntitiesModelHandlerConfig(const PageEntitiesModelHandlerConfig& other);
  ~PageEntitiesModelHandlerConfig();
};

// Gets the current configuration.
const PageEntitiesModelHandlerConfig& GetPageEntitiesModelHandlerConfig();

// Overrides the config returned by |GetPageEntitiesModelHandlerConfig()|.
void SetPageEntitiesModelHandlerConfigForTesting(
    const PageEntitiesModelHandlerConfig& config);

// An object used to hold an entity annotator on a background thread.
class EntityAnnotatorHolder {
 public:
  EntityAnnotatorHolder(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
      bool should_reset_entity_annotator_on_shutdown);
  ~EntityAnnotatorHolder();

  // Initializes the native library on a background thread. Will invoke
  // |init_callback| on |reply_task_runner_| with the max version supported for
  // the entity annotator on success. Otherwise, -1.
  void InitializeEntityAnnotatorNativeLibraryOnBackgroundThread(
      bool should_provide_filter_path,
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
      PageEntitiesModelHandler::PageEntitiesModelEntityMetadataRetrievedCallback
          callback);

  // Gets the weak ptr to |this| on the background thread.
  base::WeakPtr<EntityAnnotatorHolder> GetBackgroundWeakPtr();

 private:
  void ResetEntityAnnotator();

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;

  // Whether the entity annotator should be reset on shutdown.
  const bool should_reset_entity_annotator_on_shutdown_;

  std::unique_ptr<EntityAnnotatorNativeLibrary>
      entity_annotator_native_library_;
  raw_ptr<void> entity_annotator_ = nullptr;

  base::WeakPtrFactory<EntityAnnotatorHolder> background_weak_ptr_factory_{
      this};
};

// Manages the loading and execution of the page entities model.
class PageEntitiesModelHandlerImpl : public OptimizationTargetModelObserver,
                                     public PageEntitiesModelHandler {
 public:
  PageEntitiesModelHandlerImpl(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~PageEntitiesModelHandlerImpl() override;
  PageEntitiesModelHandlerImpl(const PageEntitiesModelHandlerImpl&) = delete;
  PageEntitiesModelHandlerImpl& operator=(const PageEntitiesModelHandlerImpl&) =
      delete;

  // PageEntitiesModelHandler:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      PageEntitiesModelEntityMetadataRetrievedCallback callback) override;
  void ExecuteModelWithInput(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback) override;
  void AddOnModelUpdatedCallback(base::OnceClosure callback) override;
  absl::optional<ModelInfo> GetModelInfo() const override;

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      base::optional_ref<const ModelInfo> model_info) override;

 private:
  // Invoked on the UI thread when entity annotator library has been
  // initialized.
  void OnEntityAnnotatorLibraryInitialized(
      OptimizationGuideModelProvider* model_provider,
      int32_t max_model_format_feature_flag);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The holder used to hold the annotator used to annotate entities.
  std::unique_ptr<EntityAnnotatorHolder> entity_annotator_holder_;

  // The most recent model info given to |OnModelUpdated|.
  absl::optional<ModelInfo> model_info_;

  // Populated with callbacks if |AddOnModelUpdatedCallback| is called before a
  // model file is available, then is notified when |OnModelUpdated| is called.
  base::OnceClosureList on_model_updated_callbacks_;

  // The opt guide model provider that gives the model updates. Populated only
  // when model observer was registered.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  base::WeakPtrFactory<PageEntitiesModelHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_IMPL_H_
