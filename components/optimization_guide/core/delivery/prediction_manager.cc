// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/core/delivery/prediction_model_download_manager.h"
#include "components/optimization_guide/core/delivery/prediction_model_fetcher_impl.h"
#include "components/optimization_guide/core/delivery/prediction_model_override.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

proto::ModelCacheKey GetModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

void RecordModelUpdateVersion(const proto::ModelInfo& model_info) {
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelUpdateVersion." +
          GetStringNameForOptimizationTarget(model_info.optimization_target()),
      model_info.version());
}

void RecordModelAvailableAtRegistration(
    proto::OptimizationTarget optimization_target,
    bool model_available_at_registration) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration." +
          GetStringNameForOptimizationTarget(optimization_target),
      model_available_at_registration);
}

}  // namespace

PredictionManager::PredictionManager(
    PredictionModelStore* prediction_model_store,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state,
    const std::string& application_locale,
    OptimizationGuideLogger* optimization_guide_logger,
    unzip::UnzipperFactory unzipper_factory)
    : registry_(optimization_guide_logger),
      prediction_model_store_(prediction_model_store),
      url_loader_factory_(url_loader_factory),
      optimization_guide_logger_(optimization_guide_logger),
      unzipper_factory_(std::move(unzipper_factory)),
      default_model_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      prediction_model_fetch_timer_(
          local_state,
          base::BindRepeating(
              &PredictionManager::FetchModels,
              // Its safe to use `base::Unretained(this)` here since
              // `prediction_model_fetch_timer_` is owned by `this`.
              base::Unretained(this))),
      application_locale_(application_locale),
      model_cache_key_(GetModelCacheKey(application_locale_)) {
  DCHECK(prediction_model_store_);
  LoadPredictionModels(GetRegisteredOptimizationTargets());
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.StoreInitialized", true);
}

PredictionManager::~PredictionManager() {
  if (prediction_model_download_manager_) {
    prediction_model_download_manager_->RemoveObserver(this);
  }
}

void PredictionManager::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const std::optional<proto::Any>& model_metadata,
    scoped_refptr<base::SequencedTaskRunner> model_task_runner,
    OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set the target's task runner if none is already present. This has the
  // effect of using the first registered model_task_runner for all model
  // execution, if more than one observer gets registered (e.g. in the case of
  // multiple profiles using the model).
  optimization_target_model_task_runner_.emplace(optimization_target,
                                                 model_task_runner);

  registry_.AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, model_task_runner, observer);
  base::UmaHistogramMediumTimes(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit." +
          GetStringNameForOptimizationTarget(optimization_target),
      !init_time_.is_null() ? base::TimeTicks::Now() - init_time_
                            : base::TimeDelta());

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Registered new OptimizationTarget: " << optimization_target;
  }

  if (prediction_model_download_manager_ &&
      prediction_model_download_manager_->ShouldFetchModels()) {
    prediction_model_fetch_timer_.ScheduleFetchOnModelRegistration();
  }

  // Otherwise, load prediction models for any newly registered targets.
  LoadPredictionModels({optimization_target});
}

void PredictionManager::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registry_.RemoveObserverForOptimizationTargetModel(optimization_target,
                                                     observer);
}

void PredictionManager::SetModelDownloadSchedulingParams(
    proto::OptimizationTarget optimization_target,
    const download::SchedulingParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  custom_scheduling_params_.insert_or_assign(optimization_target, params);
}

base::flat_set<proto::OptimizationTarget>
PredictionManager::GetRegisteredOptimizationTargets() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registry_.GetRegisteredOptimizationTargets();
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::SetPredictionModelDownloadManagerForTesting(
    std::unique_ptr<PredictionModelDownloadManager>
        prediction_model_download_manager) {
  prediction_model_download_manager_ =
      std::move(prediction_model_download_manager);
  init_time_ = base::TimeTicks::Now();
}

void PredictionManager::FetchModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT("optimization_guide", "PredictionManager::FetchModels");

  // The histogram that gets recorded here is used for integration tests that
  // pass in a model override. For simplicity, we place the recording of this
  // histogram here rather than somewhere else earlier in the session
  // initialization flow since the model engine version needs to continuously be
  // updated for the fetch.
  proto::ModelInfo base_model_info;
  // There should only be one supported model engine version at a time.
  base_model_info.add_supported_model_engine_versions(
      proto::MODEL_ENGINE_VERSION_TFLITE_2_20_2);
  // This histogram is used for integration tests. Do not remove.
  // Update this to be 10000 if/when we exceed 100 model engine versions.
  LOCAL_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion",
      static_cast<int>(
          *base_model_info.supported_model_engine_versions().begin()));

  if (!prediction_model_download_manager_ ||
      !prediction_model_download_manager_->ShouldFetchModels()) {
    return;
  }

  if (prediction_model_fetch_timer_.IsFirstModelFetch()) {
    DCHECK(!init_time_.is_null());
    base::UmaHistogramMediumTimes(
        "OptimizationGuide.PredictionManager.FirstModelFetchSinceServiceInit",
        base::TimeTicks::Now() - init_time_);
  }

  // Models should not be fetched if there are no optimization targets
  // registered.
  if (!registry_.HasRegistrations()) {
    return;
  }
  auto targets = registry_.GetRegisteredOptimizationTargets();

  // We should have already created a prediction model download manager if we
  // initiated the fetching of models.
  bool download_service_available =
      prediction_model_download_manager_->IsAvailableForDownloads();
  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      !download_service_available);
  if (!download_service_available) {
    for (const proto::OptimizationTarget target : targets) {
      ModelProviderRegistry::RecordLifecycleState(
          target, ModelDeliveryEvent::kDownloadServiceUnavailable);
    }
    // We cannot download any models from the server, so don't refresh them.
    return;
  }
  prediction_model_download_manager_->CancelAllPendingDownloads();

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();
  models_info.reserve(targets.size());

  // For now, we will fetch for all registered optimization targets.
  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  for (const proto::OptimizationTarget target : targets) {
    if (overrides.Get(target)) {
      // Do not download models that were overridden.
      continue;
    }

    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(target);
    if (auto metadata = registry_.GetRegistrationMetadata(target); metadata) {
      *model_info.mutable_model_metadata() = *metadata;
    }

    if (const ModelInfo* info = registry_.GetModel(target); info) {
      model_info.set_version(info->GetVersion());
    }

    models_info.push_back(model_info);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "Fetching models for Optimization Target "
          << model_info.optimization_target();
    }
    ModelProviderRegistry::RecordLifecycleState(
        target, ModelDeliveryEvent::kGetModelsRequest);
  }
  if (models_info.empty()) {
    return;
  }

  // NOTE: ALL PRECONDITIONS FOR THIS FUNCTION MUST BE CHECKED ABOVE THIS LINE.
  // It is assumed that if we proceed past here, that a fetch will at least be
  // attempted.

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcherImpl>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL());
  }

  bool fetch_initiated =
      prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
          models_info, proto::CONTEXT_BATCH_UPDATE_MODELS, application_locale_,
          base::BindOnce(&PredictionManager::OnModelsFetched,
                         ui_weak_ptr_factory_.GetWeakPtr(), models_info));

  if (fetch_initiated) {
    prediction_model_fetch_timer_.NotifyModelFetchAttempt();
  }
  // Schedule the next fetch regardless since we may not have initiated a fetch
  // due to a network condition and trying in the next minute to see if that is
  // unblocked is only a timer firing and not an actual query to the server.
  prediction_model_fetch_timer_.SchedulePeriodicModelsFetch();
}

scoped_refptr<base::SequencedTaskRunner> PredictionManager::GetModelTaskRunner(
    proto::OptimizationTarget optimization_target) {
  const auto loc =
      optimization_target_model_task_runner_.find(optimization_target);
  return loc != optimization_target_model_task_runner_.end()
             ? loc->second
             : default_model_task_runner_;
}

void PredictionManager::OnModelsFetched(
    const std::vector<proto::ModelInfo> models_request_info,
    std::unique_ptr<proto::GetModelsResponse> get_models_response_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT("optimization_guide", "PredictionManager::OnModelsFetched");

  if (!get_models_response_data) {
    for (const auto& model_info : models_request_info) {
      ModelProviderRegistry::RecordLifecycleState(
          model_info.optimization_target(),
          ModelDeliveryEvent::kGetModelsResponseFailure);
    }
    return;
  }

  if (get_models_response_data->models_size() > 0 ||
      models_request_info.size() > 0) {
    UpdatePredictionModels(models_request_info,
                           get_models_response_data->models());
  }

  prediction_model_fetch_timer_.NotifyModelFetchSuccess();
  prediction_model_fetch_timer_.Stop();
  prediction_model_fetch_timer_.SchedulePeriodicModelsFetch();
}

void PredictionManager::UpdateModelMetadata(
    const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Model update is needed when download URL is set, which indicates the model
  // has changed.
  if (model.model().download_url().empty()) {
    return;
  }
  if (!model.model_info().has_model_cache_key()) {
    return;
  }
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Optimization Target: " << model.model_info().optimization_target()
        << " for locale: " << application_locale_
        << " sharing models with locale: "
        << model.model_info().model_cache_key().locale();
  }
  prediction_model_store_->UpdateModelCacheKeyMapping(
      model.model_info().optimization_target(), model_cache_key_,
      model.model_info().model_cache_key());
}

bool PredictionManager::ShouldDownloadNewModel(
    const proto::PredictionModel& model) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No download needed if URL is not set.
  if (model.model().download_url().empty()) {
    return false;
  }
  // Though the server set the download URL indicating the model is old or does
  // not exist in client, the same version model could exist in the store, if
  // the model is shared across different profile characteristics, based on
  // ModelCacheKey. So, download only when the same version is not found in the
  // store.
  return !prediction_model_store_->HasModelWithVersion(
      model.model_info().optimization_target(), model_cache_key_,
      model.model_info().version());
}

void PredictionManager::StartModelDownload(
    proto::OptimizationTarget optimization_target,
    const GURL& download_url) {
  TRACE_EVENT("optimization_guide", "PredictionManager::StartModelDownload",
              "target",
              GetStringNameForOptimizationTarget(optimization_target));

  if (download_url.is_valid()) {
    std::optional<download::SchedulingParams> scheduling_params;
    auto it = custom_scheduling_params_.find(optimization_target);
    if (it != custom_scheduling_params_.end()) {
      scheduling_params = it->second;
    }
    prediction_model_download_manager_->StartDownload(
        download_url, optimization_target, scheduling_params);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "Model download started for Optimization Target: "
          << optimization_target << " download URL: " << download_url;
    }
  }
  ModelProviderRegistry::RecordLifecycleState(
      optimization_target, download_url.is_valid()
                               ? ModelDeliveryEvent::kDownloadServiceRequest
                               : ModelDeliveryEvent::kDownloadURLInvalid);
}

void PredictionManager::MaybeDownloadOrUpdatePredictionModel(
    proto::OptimizationTarget optimization_target,
    const proto::PredictionModel& get_models_response_model,
    std::unique_ptr<proto::PredictionModel> loaded_model) {
  if (!loaded_model) {
    // Model load failed, redownload the model.
    ModelProviderRegistry::RecordLifecycleState(
        optimization_target, ModelDeliveryEvent::kModelLoadFailed);
    DCHECK(!get_models_response_model.model().download_url().empty());
    StartModelDownload(optimization_target,
                       GURL(get_models_response_model.model().download_url()));
    return;
  }
  prediction_model_store_->UpdateMetadataForExistingModel(
      optimization_target, model_cache_key_,
      get_models_response_model.model_info());
  OnLoadPredictionModel(optimization_target,
                        /*record_availability_metrics=*/false,
                        std::move(loaded_model));
}

void PredictionManager::UpdatePredictionModels(
    const std::vector<proto::ModelInfo>& models_request_info,
    const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
        prediction_models) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT("optimization_guide",
              "PredictionManager::UpdatePredictionModels");

  std::set<proto::OptimizationTarget> received_optimization_targets;
  for (const auto& model : prediction_models) {
    auto optimization_target = model.model_info().optimization_target();
    received_optimization_targets.emplace(optimization_target);
    if (!model.has_model()) {
      // We already have this updated model, so don't update in store.
      continue;
    }
    DCHECK(!model.model().download_url().empty());
    UpdateModelMetadata(model);
    if (ShouldDownloadNewModel(model)) {
      StartModelDownload(optimization_target,
                         GURL(model.model().download_url()));
      // Skip over models that have a download URL since they will be updated
      // once the download has completed successfully.
      continue;
    }

    RecordModelUpdateVersion(model.model_info());
    DCHECK(prediction_model_store_->HasModel(optimization_target,
                                             model_cache_key_));
    // Load the model from the store to see whether it is valid or not.
    prediction_model_store_->LoadModel(
        optimization_target, model_cache_key_,
        GetModelTaskRunner(optimization_target),
        base::BindOnce(&PredictionManager::MaybeDownloadOrUpdatePredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                       model));
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "Model Download Not Required for target: " << optimization_target
          << "\nNew Version: "
          << base::NumberToString(model.model_info().version());
    }
  }

  for (const auto& model_info : models_request_info) {
    if (received_optimization_targets.find(model_info.optimization_target()) ==
        received_optimization_targets.end()) {
      RemoveModelFromStore(
          model_info.optimization_target(),
          PredictionModelStoreModelRemovalReason::kNoModelInGetModelsResponse);
    }
  }
}

void PredictionManager::OnModelReady(const base::FilePath& base_model_dir,
                                     const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model.model_info().has_version() &&
         model.model_info().has_optimization_target());

  TRACE_EVENT("optimization_guide", "PredictionManager::OnModelReady", "target",
              GetStringNameForOptimizationTarget(
                  model.model_info().optimization_target()));

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  if (overrides.Get(model.model_info().optimization_target())) {
    // Skip updating the model if override is present.
    return;
  }

  RecordModelUpdateVersion(model.model_info());
  ModelProviderRegistry::RecordLifecycleState(
      model.model_info().optimization_target(),
      ModelDeliveryEvent::kModelDownloaded);
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Model Files Downloaded target: "
        << model.model_info().optimization_target()
        << "\nNew Version: " +
               base::NumberToString(model.model_info().version());
  }

  // Store the received model in the store.
  prediction_model_store_->UpdateModel(
      model.model_info().optimization_target(), model_cache_key_,
      model.model_info(), base_model_dir,
      base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));

  if (registry_.IsRegistered(model.model_info().optimization_target())) {
    OnLoadPredictionModel(model.model_info().optimization_target(),
                          /*record_availability_metrics=*/false,
                          std::make_unique<proto::PredictionModel>(model));
  }
}

void PredictionManager::OnModelDownloadStarted(
    proto::OptimizationTarget optimization_target) {
  TRACE_EVENT("optimization_guide", "PredictionManager::OnModelDownloadStarted",
              "target",
              GetStringNameForOptimizationTarget(optimization_target));
  ModelProviderRegistry::RecordLifecycleState(
      optimization_target, ModelDeliveryEvent::kModelDownloadStarted);
}

void PredictionManager::OnModelDownloadFailed(
    proto::OptimizationTarget optimization_target) {
  TRACE_EVENT("optimization_guide", "PredictionManager::OnModelDownloadFailed",
              "target",
              GetStringNameForOptimizationTarget(optimization_target));
  ModelProviderRegistry::RecordLifecycleState(
      optimization_target, ModelDeliveryEvent::kModelDownloadFailure);
}

std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
PredictionManager::GetDownloadedModelsInfoForWebUI() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registry_.GetDownloadedModelsInfoForWebUI();
}

base::flat_map<std::string, bool>
PredictionManager::GetOnDeviceSupplementaryModelsInfoForWebUI() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<proto::OptimizationTarget> supp_targets = {
      proto::OptimizationTarget::OPTIMIZATION_TARGET_TEXT_SAFETY,
      proto::OptimizationTarget::OPTIMIZATION_TARGET_GENERALIZED_SAFETY,
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION};
  base::flat_map<std::string, bool> supp_models_info;
  for (const auto target : supp_targets) {
    supp_models_info[proto::OptimizationTarget_Name(target)] =
        !!registry_.GetModel(target);
  }

  return supp_models_info;
}

void PredictionManager::OnPredictionModelsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::MaybeInitializeModelDownloads(
    ProfileDownloadServiceTracker& download_service_tracker,
    PrefService* local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_time_ = base::TimeTicks::Now();
  if (!prediction_model_download_manager_) {
    prediction_model_download_manager_ =
        std::make_unique<PredictionModelDownloadManager>(
            local_state, download_service_tracker,
            base::BindRepeating(
                &PredictionManager::GetBaseModelDirForDownload,
                // base::Unretained is safe here because the
                // PredictionModelDownloadManager is owned by `this`
                base::Unretained(this)),
            unzipper_factory_,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
    prediction_model_download_manager_->AddObserver(this);
  }

  // Only load models if there are optimization targets registered.
  if (registry_.HasRegistrations() &&
      prediction_model_download_manager_->ShouldFetchModels()) {
    prediction_model_fetch_timer_.MaybeScheduleFirstModelFetch();
  }
}

void PredictionManager::OnPredictionModelOverrideLoaded(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<proto::PredictionModel> prediction_model) {
  const bool is_available = prediction_model != nullptr;
  VLOG(0) << "Loading override for "
          << proto::OptimizationTarget_Name(optimization_target)
          << (is_available ? " succeeded" : " failed");
  OnLoadPredictionModel(optimization_target,
                        /*record_availability_metrics=*/false,
                        std::move(prediction_model));
  RecordModelAvailableAtRegistration(optimization_target, is_available);
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  for (proto::OptimizationTarget optimization_target : optimization_targets) {
    // Give preference to any overrides given on the command line.
    if (auto* entry = overrides.Get(optimization_target); entry) {
      base::FilePath base_model_dir =
          GetBaseModelDirForDownload(optimization_target);
      entry->BuildModel(
          base_model_dir, unzipper_factory_,
          base::BindOnce(&PredictionManager::OnPredictionModelOverrideLoaded,
                         ui_weak_ptr_factory_.GetWeakPtr(),
                         optimization_target));
      continue;
    }

    if (!prediction_model_store_->HasModel(optimization_target,
                                           model_cache_key_)) {
      RecordModelAvailableAtRegistration(optimization_target, false);
      continue;
    }
    prediction_model_store_->LoadModel(
        optimization_target, model_cache_key_,
        GetModelTaskRunner(optimization_target),
        base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                       /*record_availability_metrics=*/true));
  }
}

void PredictionManager::OnLoadPredictionModel(
    proto::OptimizationTarget optimization_target,
    bool record_availability_metrics,
    std::unique_ptr<proto::PredictionModel> model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model) {
    if (record_availability_metrics) {
      RecordModelAvailableAtRegistration(optimization_target, false);
    }
    return;
  }
  bool success = ProcessAndStoreLoadedModel(*model);
  DCHECK_EQ(optimization_target, model->model_info().optimization_target());
  if (record_availability_metrics) {
    RecordModelAvailableAtRegistration(optimization_target, success);
  }
  OnProcessLoadedModel(*model, success);
}

void PredictionManager::OnProcessLoadedModel(
    const proto::PredictionModel& model,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    base::UmaHistogramSparse("OptimizationGuide.PredictionModelLoadedVersion." +
                                 GetStringNameForOptimizationTarget(
                                     model.model_info().optimization_target()),
                             model.model_info().version());
    return;
  }
  RemoveModelFromStore(
      model.model_info().optimization_target(),
      PredictionModelStoreModelRemovalReason::kModelLoadFailed);
}

void PredictionManager::RemoveModelFromStore(
    proto::OptimizationTarget optimization_target,
    PredictionModelStoreModelRemovalReason model_removal_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (prediction_model_store_->HasModel(optimization_target,
                                        model_cache_key_)) {
    prediction_model_store_->RemoveModel(optimization_target, model_cache_key_,
                                         model_removal_reason);
    registry_.RemoveModel(optimization_target);
  }
}

bool PredictionManager::ProcessAndStoreLoadedModel(
    const proto::PredictionModel& model) {
  TRACE_EVENT("optimization_guide",
              "PredictionManager::ProcessAndStoreLoadedModel");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.model_info().has_optimization_target()) {
    return false;
  }
  if (!model.model_info().has_version()) {
    return false;
  }
  if (!model.has_model()) {
    return false;
  }

  proto::OptimizationTarget optimization_target =
      model.model_info().optimization_target();
  if (!registry_.IsRegistered(optimization_target)) {
    return false;
  }

  std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);

  base::UmaHistogramBoolean(
      "OptimizationGuide.IsPredictionModelValid." +
          GetStringNameForOptimizationTarget(optimization_target),
      !!model_info);

  if (!model_info) {
    return false;
  }

  // See if we should update the loaded model.
  if (!ShouldUpdateStoredModelForTarget(optimization_target,
                                        model.model_info().version())) {
    return true;
  }

  // Update prediction model file if that is what we have loaded.
  if (model_info) {
    StoreLoadedModelInfo(optimization_target, std::move(model_info));
  }

  return true;
}

bool PredictionManager::ShouldUpdateStoredModelForTarget(
    proto::OptimizationTarget optimization_target,
    int64_t new_version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const ModelInfo* info = registry_.GetModel(optimization_target); info) {
    return info->GetVersion() != new_version;
  }
  return true;
}

void PredictionManager::StoreLoadedModelInfo(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_info);
  registry_.UpdateModel(optimization_target, std::move(model_info));
}

base::FilePath PredictionManager::GetBaseModelDirForDownload(
    proto::OptimizationTarget optimization_target) {
  return prediction_model_store_->GetBaseModelDirForModelCacheKey(
      optimization_target, model_cache_key_);
}

void PredictionManager::OverrideTargetModelForTesting(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (model_info) {
    registry_.UpdateModelImmediatelyForTesting(  // IN-TEST
        optimization_target, std::move(model_info));
  } else {
    registry_.RemoveModel(optimization_target);
  }
}

void PredictionManager::SetUrlLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

}  // namespace optimization_guide
