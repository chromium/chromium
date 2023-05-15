// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_entities_model_handler_impl.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/entity_annotator_native_library.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"

namespace optimization_guide {

namespace {

const char kPageEntitiesModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata";

// The max number of page entities that should be output.
// TODO(crbug/1234578): Make the number of entities Finch-able once we
// know how much the model is expected to output.
constexpr size_t kMaxPageEntities = 5;

PageEntitiesModelHandlerConfig& GetPageEntitiesModelHandlerConfigInternal() {
  static base::NoDestructor<PageEntitiesModelHandlerConfig> s_config;
  return *s_config;
}

}  // namespace

PageEntitiesModelHandlerConfig::PageEntitiesModelHandlerConfig() {
  // Override any parameters that may be provided by Finch.
  should_reset_entity_annotator_on_shutdown =
      base::FeatureList::IsEnabled(features::kPageEntitiesModelResetOnShutdown);

  should_provide_filter_path =
      !base::FeatureList::IsEnabled(features::kPageEntitiesModelBypassFilters);
}

PageEntitiesModelHandlerConfig::PageEntitiesModelHandlerConfig(
    const PageEntitiesModelHandlerConfig& other) = default;
PageEntitiesModelHandlerConfig::~PageEntitiesModelHandlerConfig() = default;

void SetPageEntitiesModelHandlerConfigForTesting(
    const PageEntitiesModelHandlerConfig& config) {
  GetPageEntitiesModelHandlerConfigInternal() = config;
}

const PageEntitiesModelHandlerConfig& GetPageEntitiesModelHandlerConfig() {
  return GetPageEntitiesModelHandlerConfigInternal();
}

EntityAnnotatorHolder::EntityAnnotatorHolder(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    bool should_reset_entity_annotator_on_shutdown)
    : background_task_runner_(background_task_runner),
      reply_task_runner_(reply_task_runner),
      should_reset_entity_annotator_on_shutdown_(
          should_reset_entity_annotator_on_shutdown) {}

EntityAnnotatorHolder::~EntityAnnotatorHolder() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (should_reset_entity_annotator_on_shutdown_) {
    ResetEntityAnnotator();
  }
}

void EntityAnnotatorHolder::
    InitializeEntityAnnotatorNativeLibraryOnBackgroundThread(
        bool should_provide_filter_path,
        base::OnceCallback<void(int32_t)> init_callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!entity_annotator_native_library_);
  if (entity_annotator_native_library_) {
    // We should only be initialized once but in case someone does something
    // wrong in a non-debug build, we invoke the callback anyway.
    reply_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(init_callback),
            entity_annotator_native_library_->GetMaxSupportedFeatureFlag()));
    return;
  }

  entity_annotator_native_library_ =
      EntityAnnotatorNativeLibrary::Create(should_provide_filter_path);
  if (!entity_annotator_native_library_) {
    reply_task_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(init_callback), -1));
    return;
  }

  int32_t max_supported_feature_flag =
      entity_annotator_native_library_->GetMaxSupportedFeatureFlag();
  reply_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(init_callback), max_supported_feature_flag));
}

void EntityAnnotatorHolder::ResetEntityAnnotator() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    entity_annotator_native_library_->DeleteEntityAnnotator(entity_annotator_);

    entity_annotator_ = nullptr;
  }
}

void EntityAnnotatorHolder::CreateAndSetEntityAnnotatorOnBackgroundThread(
    const ModelInfo& model_info) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!entity_annotator_native_library_) {
    return;
  }

  ResetEntityAnnotator();

  entity_annotator_ =
      entity_annotator_native_library_->CreateEntityAnnotator(model_info);
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageEntitiesModelHandler.CreatedSuccessfully",
      entity_annotator_ != nullptr);
}

void EntityAnnotatorHolder::AnnotateEntitiesMetadataModelOnBackgroundThread(
    const std::string& text,
    PageEntitiesMetadataModelExecutedCallback callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::ElapsedThreadTimer annotate_timer;

  absl::optional<std::vector<ScoredEntityMetadata>> scored_md;
  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    base::TimeTicks start_time = base::TimeTicks::Now();
    scored_md =
        entity_annotator_native_library_->AnnotateText(entity_annotator_, text);
    // The max of the below histograms is 1 hour because we want to understand
    // tail behavior and catch long running model executions.
    base::UmaHistogramLongTimes(
        "OptimizationGuide.PageContentAnnotationsService.ModelExecutionLatency."
        "PageEntities",
        base::TimeTicks::Now() - start_time);

    base::UmaHistogramLongTimes(
        "OptimizationGuide.PageContentAnnotationsService."
        "ModelThreadExecutionLatency.PageEntities",
        annotate_timer.Elapsed());
  }

  if (scored_md) {
    // Determine the entities with the highest weights.
    std::sort(scored_md->begin(), scored_md->end(),
              [](const ScoredEntityMetadata& a, const ScoredEntityMetadata& b) {
                return a.score > b.score;
              });

    // Limit the output to the top |kMaxPageEntities| items.
    if (scored_md->size() > kMaxPageEntities) {
      scored_md->resize(kMaxPageEntities);
    }
  }
  reply_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), scored_md));
}

void EntityAnnotatorHolder::GetMetadataForEntityIdOnBackgroundThread(
    const std::string& entity_id,
    PageEntitiesModelHandler::PageEntitiesModelEntityMetadataRetrievedCallback
        callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  absl::optional<EntityMetadata> entity_metadata;
  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    entity_metadata =
        entity_annotator_native_library_->GetEntityMetadataForEntityId(
            entity_annotator_, entity_id);
  }
  reply_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(entity_metadata)));
}

base::WeakPtr<EntityAnnotatorHolder>
EntityAnnotatorHolder::GetBackgroundWeakPtr() {
  return background_weak_ptr_factory_.GetWeakPtr();
}

PageEntitiesModelHandlerImpl::PageEntitiesModelHandlerImpl(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner),
      entity_annotator_holder_(std::make_unique<EntityAnnotatorHolder>(
          background_task_runner_,
          base::SequencedTaskRunner::GetCurrentDefault(),
          GetPageEntitiesModelHandlerConfig()
              .should_reset_entity_annotator_on_shutdown)) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::
              InitializeEntityAnnotatorNativeLibraryOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(),
          GetPageEntitiesModelHandlerConfig().should_provide_filter_path,
          base::BindOnce(&PageEntitiesModelHandlerImpl::
                             OnEntityAnnotatorLibraryInitialized,
                         weak_ptr_factory_.GetWeakPtr(),
                         optimization_guide_model_provider)));
}

void PageEntitiesModelHandlerImpl::OnEntityAnnotatorLibraryInitialized(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    int32_t max_model_format_feature_flag) {
  if (max_model_format_feature_flag <= 0) {
    return;
  }

  proto::Any any_metadata;
  any_metadata.set_type_url(kPageEntitiesModelMetadataTypeUrl);
  proto::PageEntitiesModelMetadata model_metadata;
  model_metadata.set_max_model_format_feature_flag(
      max_model_format_feature_flag);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  optimization_guide_model_provider->AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_ENTITIES,
      any_metadata, this);
}

PageEntitiesModelHandlerImpl::~PageEntitiesModelHandlerImpl() {
  // |entity_annotator_holder_|'s  WeakPtrs are used on the background thread,
  // so that is also where the class must be destroyed.
  background_task_runner_->DeleteSoon(FROM_HERE,
                                      std::move(entity_annotator_holder_));
}

void PageEntitiesModelHandlerImpl::AddOnModelUpdatedCallback(
    base::OnceClosure callback) {
  if (model_info_) {
    std::move(callback).Run();
    return;
  }

  // callbacks are not bound locally and are safe to be destroyed at any time.
  on_model_updated_callbacks_.AddUnsafe(std::move(callback));
}

void PageEntitiesModelHandlerImpl::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    const ModelInfo& model_info) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAGE_ENTITIES)
    return;

  model_info_ = model_info;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::CreateAndSetEntityAnnotatorOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(), model_info));

  // Run any observing callbacks after the model file is posted to the
  // model executor thread so that any model execution requests are posted to
  // the model executor thread after the model update.
  on_model_updated_callbacks_.Notify();
}

absl::optional<ModelInfo> PageEntitiesModelHandlerImpl::GetModelInfo() const {
  return model_info_;
}

void PageEntitiesModelHandlerImpl::ExecuteModelWithInput(
    const std::string& text,
    PageEntitiesMetadataModelExecutedCallback callback) {
  if (text.empty()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EntityAnnotatorHolder::
                         AnnotateEntitiesMetadataModelOnBackgroundThread,
                     entity_annotator_holder_->GetBackgroundWeakPtr(), text,
                     std::move(callback)));
}

void PageEntitiesModelHandlerImpl::GetMetadataForEntityId(
    const std::string& entity_id,
    PageEntitiesModelEntityMetadataRetrievedCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::GetMetadataForEntityIdOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(), entity_id,
          std::move(callback)));
}

}  // namespace optimization_guide
