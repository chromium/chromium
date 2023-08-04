// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_manager.h"

#include <memory>
#include <utility>
#include <vector>

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
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"
#include "components/optimization_guide/core/prediction_model_override.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

proto::ModelCacheKey GetModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

// Util class for recording the construction and validation of a prediction
// model. The result is recorded when it goes out of scope and its destructor is
// called.
class ScopedPredictionModelConstructionAndValidationRecorder {
 public:
  explicit ScopedPredictionModelConstructionAndValidationRecorder(
      proto::OptimizationTarget optimization_target)
      : validation_start_time_(base::TimeTicks::Now()),
        optimization_target_(optimization_target) {}

  ~ScopedPredictionModelConstructionAndValidationRecorder() {
    base::UmaHistogramBoolean(
        "OptimizationGuide.IsPredictionModelValid." +
            GetStringNameForOptimizationTarget(optimization_target_),
        is_valid_);

    // Only record the timing if the model is valid and was able to be
    // constructed.
    if (is_valid_) {
      base::TimeDelta validation_latency =
          base::TimeTicks::Now() - validation_start_time_;
      base::UmaHistogramTimes(
          "OptimizationGuide.PredictionModelValidationLatency." +
              GetStringNameForOptimizationTarget(optimization_target_),
          validation_latency);
    }
  }

  void set_is_valid(bool is_valid) { is_valid_ = is_valid; }

 private:
  bool is_valid_ = true;
  const base::TimeTicks validation_start_time_;
  const proto::OptimizationTarget optimization_target_;
};

void RecordModelUpdateVersion(const proto::ModelInfo& model_info) {
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelUpdateVersion." +
          GetStringNameForOptimizationTarget(model_info.optimization_target()),
      model_info.version());
}

void RecordLifecycleState(proto::OptimizationTarget optimization_target,
                          ModelDeliveryEvent event) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents." +
          GetStringNameForOptimizationTarget(optimization_target),
      event);
}

// Returns whether models should be fetched from the
// remote Optimization Guide Service.
bool ShouldFetchModels(bool off_the_record, bool component_updates_enabled) {
  return features::IsRemoteFetchingEnabled() && !off_the_record &&
         features::IsModelDownloadingEnabled() && component_updates_enabled;
}

// Returns whether the model metadata proto is on the server allowlist.
bool IsModelMetadataTypeOnServerAllowlist(const proto::Any& model_metadata) {
  return model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.internal.chrome.optimizationguide.v1."
             "OnDeviceTailSuggestModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.internal.chrome.optimizationguide.v1."
             "PageEntitiesModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.internal.chrome.optimizationguide.v1."
             "PageTopicsModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.internal.chrome.optimizationguide.v1."
             "SegmentationModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.privacy.webpermissionpredictions.v1."
             "WebPermissionPredictionsModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "google.internal.chrome.optimizationguide.v1."
             "ClientSidePhishingModelMetadata" ||
         model_metadata.type_url() ==
             "type.googleapis.com/"
             "lens.prime.csc.VisualSearchModelMetadata";
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
    base::WeakPtr<OptimizationGuideStore> model_and_features_store,
    PredictionModelStore* prediction_model_store,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    bool off_the_record,
    const std::string& application_locale,
    const base::FilePath& models_dir_path,
    OptimizationGuideLogger* optimization_guide_logger,
    BackgroundDownloadServiceProvider background_download_service_provider,
    ComponentUpdatesEnabledProvider component_updates_enabled_provider)
    : prediction_model_download_manager_(nullptr),
      model_and_features_store_(model_and_features_store),
      prediction_model_store_(prediction_model_store),
      url_loader_factory_(url_loader_factory),
      optimization_guide_logger_(optimization_guide_logger),
      pref_service_(pref_service),
      component_updates_enabled_provider_(component_updates_enabled_provider),
      prediction_model_fetch_timer_(
          pref_service,
          base::BindRepeating(
              &PredictionManager::FetchModels,
              // Its safe to use `base::Unretained(this)` here since
              // `prediction_model_fetch_timer_` is owned by `this`.
              base::Unretained(this))),
      clock_(base::DefaultClock::GetInstance()),
      off_the_record_(off_the_record),
      application_locale_(application_locale),
      model_cache_key_(GetModelCacheKey(application_locale_)),
      models_dir_path_(models_dir_path) {
  DCHECK(!features::IsInstallWideModelStoreEnabled() ||
         prediction_model_store_);
  DCHECK(!model_and_features_store_ || !prediction_model_store_);
  Initialize(std::move(background_download_service_provider));
}

PredictionManager::~PredictionManager() {
  if (prediction_model_download_manager_)
    prediction_model_download_manager_->RemoveObserver(this);
}

void PredictionManager::Initialize(
    BackgroundDownloadServiceProvider background_download_service_provider) {
  if (features::IsInstallWideModelStoreEnabled()) {
    store_is_ready_ = true;
    LoadPredictionModels(GetRegisteredOptimizationTargets());
    LOCAL_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.PredictionManager.StoreInitialized", true);
  } else if (model_and_features_store_) {
    model_and_features_store_->Initialize(
        switches::ShouldPurgeModelAndFeaturesStoreOnStartup(),
        base::BindOnce(&PredictionManager::OnStoreInitialized,
                       ui_weak_ptr_factory_.GetWeakPtr(),
                       std::move(background_download_service_provider)));
  }
}

void PredictionManager::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const absl::optional<proto::Any>& model_metadata,
    OptimizationTargetModelObserver* observer) {
  DCHECK(registered_observers_for_optimization_targets_.find(
             optimization_target) ==
         registered_observers_for_optimization_targets_.end());

  // As DCHECKS don't run in the wild, just do not register the observer if
  // something is already registered for the type. Otherwise, file reads may
  // blow up.
  if (registered_observers_for_optimization_targets_.find(
          optimization_target) !=
      registered_observers_for_optimization_targets_.end()) {
    DLOG(ERROR) << "Did not add observer for optimization target "
                << static_cast<int>(optimization_target)
                << " since an observer for the target was already registered ";
    return;
  }

  registered_observers_for_optimization_targets_[optimization_target]
      .AddObserver(observer);
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Observer added for OptimizationTarget: " << optimization_target;
  }

  // Notify observer of existing model file path.
  auto model_it = optimization_target_model_info_map_.find(optimization_target);
  if (model_it != optimization_target_model_info_map_.end()) {
    observer->OnModelUpdated(optimization_target, *model_it->second);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "OnModelFileUpdated for OptimizationTarget: "
          << optimization_target << "\nFile path: "
          << (*model_it->second).GetModelFilePath().AsUTF8Unsafe()
          << "\nHas metadata: " << (model_metadata ? "True" : "False");
    }
    RecordLifecycleState(optimization_target,
                         ModelDeliveryEvent::kModelDeliveredAtRegistration);
  }
  base::UmaHistogramMediumTimes(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit." +
          GetStringNameForOptimizationTarget(optimization_target),
      !init_time_.is_null() ? base::TimeTicks::Now() - init_time_
                            : base::TimeDelta());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (registered_optimization_targets_and_metadata_.contains(
          optimization_target)) {
    return;
  }
  DCHECK(!model_metadata ||
         IsModelMetadataTypeOnServerAllowlist(*model_metadata));

  registered_optimization_targets_and_metadata_.emplace(optimization_target,
                                                        model_metadata);
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Registered new OptimizationTarget: " << optimization_target;
  }

  // Before loading/fetching models and features, the store must be ready.
  if (!store_is_ready_)
    return;

  if (ShouldFetchModels(off_the_record_,
                        component_updates_enabled_provider_.Run())) {
    prediction_model_fetch_timer_.ScheduleFetchOnModelRegistration();
  }

  // Otherwise, load prediction models for any newly registered targets.
  LoadPredictionModels({optimization_target});
}

void PredictionManager::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;
  observers_it->second.RemoveObserver(observer);
  if (observers_it->second.empty()) {
    registered_observers_for_optimization_targets_.erase(observers_it);
  }
}

base::flat_set<proto::OptimizationTarget>
PredictionManager::GetRegisteredOptimizationTargets() const {
  base::flat_set<proto::OptimizationTarget> optimization_targets;
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    optimization_targets.insert(optimization_target_and_metadata.first);
  }
  return optimization_targets;
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
}

void PredictionManager::FetchModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_is_ready_);

  // The histogram that gets recorded here is used for integration tests that
  // pass in a model override. For simplicity, we place the recording of this
  // histogram here rather than somewhere else earlier in the session
  // initialization flow since the model engine version needs to continuously be
  // updated for the fetch.
  proto::ModelInfo base_model_info;
  // There should only be one supported model engine version at a time.
  base_model_info.add_supported_model_engine_versions(
      proto::MODEL_ENGINE_VERSION_TFLITE_2_14_1);
  // This histogram is used for integration tests. Do not remove.
  // Update this to be 10000 if/when we exceed 100 model engine versions.
  LOCAL_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion",
      static_cast<int>(
          *base_model_info.supported_model_engine_versions().begin()));

  if (switches::IsModelOverridePresent())
    return;

  if (!ShouldFetchModels(off_the_record_,
                         component_updates_enabled_provider_.Run())) {
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
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  // We should have already created a prediction model download manager if we
  // initiated the fetching of models.
  DCHECK(prediction_model_download_manager_);
  if (prediction_model_download_manager_) {
    bool download_service_available =
        prediction_model_download_manager_->IsAvailableForDownloads();
    base::UmaHistogramBoolean(
        "OptimizationGuide.PredictionManager."
        "DownloadServiceAvailabilityBlockedFetch",
        !download_service_available);
    if (!download_service_available) {
      for (const auto& optimization_target_and_metadata :
           registered_optimization_targets_and_metadata_) {
        RecordLifecycleState(optimization_target_and_metadata.first,
                             ModelDeliveryEvent::kDownloadServiceUnavailable);
      }
      // We cannot download any models from the server, so don't refresh them.
      return;
    }

    prediction_model_download_manager_->CancelAllPendingDownloads();
  }

  // NOTE: ALL PRECONDITIONS FOR THIS FUNCTION MUST BE CHECKED ABOVE THIS LINE.
  // It is assumed that if we proceed past here, that a fetch will at least be
  // attempted.

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcherImpl>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL());
  }

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();
  models_info.reserve(registered_optimization_targets_and_metadata_.size());

  // For now, we will fetch for all registered optimization targets.
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(optimization_target_and_metadata.first);
    if (optimization_target_and_metadata.second.has_value()) {
      *model_info.mutable_model_metadata() =
          *optimization_target_and_metadata.second;
    }

    auto model_it = optimization_target_model_info_map_.find(
        optimization_target_and_metadata.first);
    if (model_it != optimization_target_model_info_map_.end())
      model_info.set_version(model_it->second.get()->GetVersion());

    models_info.push_back(model_info);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "Fetching models for Optimization Target "
          << model_info.optimization_target();
    }
    RecordLifecycleState(optimization_target_and_metadata.first,
                         ModelDeliveryEvent::kGetModelsRequest);
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

void PredictionManager::OnModelsFetched(
    const std::vector<proto::ModelInfo> models_request_info,
    absl::optional<std::unique_ptr<proto::GetModelsResponse>>
        get_models_response_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_is_ready_);
  if (!get_models_response_data) {
    for (const auto& model_info : models_request_info) {
      RecordLifecycleState(model_info.optimization_target(),
                           ModelDeliveryEvent::kGetModelsResponseFailure);
    }
    return;
  }

  if ((*get_models_response_data)->models_size() > 0 ||
      models_request_info.size() > 0) {
    UpdatePredictionModels(models_request_info,
                           (*get_models_response_data)->models());
  }

  prediction_model_fetch_timer_.NotifyModelFetchSuccess();
  prediction_model_fetch_timer_.Stop();
  prediction_model_fetch_timer_.SchedulePeriodicModelsFetch();
}

void PredictionManager::UpdateModelMetadata(
    const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!features::IsInstallWideModelStoreEnabled()) {
    return;
  }

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
  if (!features::IsInstallWideModelStoreEnabled()) {
    return true;
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
  // We should only be downloading models and updating the store for
  // on-the-record profiles and after the store has been initialized.
  DCHECK(prediction_model_download_manager_);
  if (!prediction_model_download_manager_) {
    return;
  }
  if (download_url.is_valid()) {
    prediction_model_download_manager_->StartDownload(download_url,
                                                      optimization_target);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "Model download started for Optimization Target: "
          << optimization_target << " download URL: " << download_url;
    }
  }
  RecordLifecycleState(optimization_target,
                       download_url.is_valid()
                           ? ModelDeliveryEvent::kDownloadServiceRequest
                           : ModelDeliveryEvent::kDownloadURLInvalid);
  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionManager.IsDownloadUrlValid." +
          GetStringNameForOptimizationTarget(optimization_target),
      download_url.is_valid());
}

void PredictionManager::MaybeDownloadOrUpdatePredictionModel(
    proto::OptimizationTarget optimization_target,
    const proto::PredictionModel& get_models_response_model,
    std::unique_ptr<proto::PredictionModel> loaded_model) {
  if (!loaded_model) {
    // Model load failed, redownload the model.
    RecordLifecycleState(optimization_target,
                         ModelDeliveryEvent::kModelLoadFailed);
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

  // Check whether the model store from the original profile still exists.
  if (!features::IsInstallWideModelStoreEnabled() && !model_and_features_store_)
    return;

  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData(
          clock_->Now() + features::StoredModelsValidDuration());
  bool has_models_to_update = false;
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
    if (features::IsInstallWideModelStoreEnabled()) {
      DCHECK(prediction_model_store_->HasModel(optimization_target,
                                               model_cache_key_));
      // Load the model from the store to see whether it is valid or not.
      prediction_model_store_->LoadModel(
          optimization_target, model_cache_key_,
          base::BindOnce(
              &PredictionManager::MaybeDownloadOrUpdatePredictionModel,
              ui_weak_ptr_factory_.GetWeakPtr(), optimization_target, model));
    } else {
      // Storing the model regardless of whether the model is valid or not.
      // Model will be removed from store if it fails to load.
      has_models_to_update = true;
      prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
      OnLoadPredictionModel(optimization_target,
                            /*record_availability_metrics=*/false,
                            std::make_unique<proto::PredictionModel>(model));
    }
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

  if (has_models_to_update && model_and_features_store_) {
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnModelReady(const base::FilePath& base_model_dir,
                                     const proto::PredictionModel& model) {
  if (switches::IsModelOverridePresent())
    return;

  if (!features::IsInstallWideModelStoreEnabled() && !model_and_features_store_)
    return;

  DCHECK(model.model_info().has_version() &&
         model.model_info().has_optimization_target());

  RecordModelUpdateVersion(model.model_info());
  RecordLifecycleState(model.model_info().optimization_target(),
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
  if (features::IsInstallWideModelStoreEnabled()) {
    prediction_model_store_->UpdateModel(
        model.model_info().optimization_target(), model_cache_key_,
        model.model_info(), base_model_dir,
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::unique_ptr<StoreUpdateData> prediction_model_update_data =
        StoreUpdateData::CreatePredictionModelStoreUpdateData(
            clock_->Now() + features::StoredModelsValidDuration());
    prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }

  if (registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    OnLoadPredictionModel(model.model_info().optimization_target(),
                          /*record_availability_metrics=*/false,
                          std::make_unique<proto::PredictionModel>(model));
  }
}

void PredictionManager::OnModelDownloadStarted(
    proto::OptimizationTarget optimization_target) {
  RecordLifecycleState(optimization_target,
                       ModelDeliveryEvent::kModelDownloadStarted);
}

void PredictionManager::OnModelDownloadFailed(
    proto::OptimizationTarget optimization_target) {
  RecordLifecycleState(optimization_target,
                       ModelDeliveryEvent::kModelDownloadFailure);
}

std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
PredictionManager::GetDownloadedModelsInfoForWebUI() const {
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info;
  downloaded_models_info.reserve(optimization_target_model_info_map_.size());
  for (const auto& it : optimization_target_model_info_map_) {
    const std::string& optimization_target_name =
        optimization_guide::proto::OptimizationTarget_Name(it.first);
    const optimization_guide::ModelInfo* const model_info = it.second.get();
    auto downloaded_model_info_ptr =
        optimization_guide_internals::mojom::DownloadedModelInfo::New(
            optimization_target_name, model_info->GetVersion(),
            model_info->GetModelFilePath().AsUTF8Unsafe());
    downloaded_models_info.push_back(std::move(downloaded_model_info_ptr));
  }
  return downloaded_models_info;
}

void PredictionManager::NotifyObserversOfNewModel(
    proto::OptimizationTarget optimization_target,
    const ModelInfo& model_info) {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;
  RecordLifecycleState(optimization_target,
                       ModelDeliveryEvent::kModelDelivered);
  for (auto& observer : observers_it->second) {
    observer.OnModelUpdated(optimization_target, model_info);
    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
          optimization_guide_logger_)
          << "OnModelFileUpdated for target: " << optimization_target
          << "\nFile path: " << model_info.GetModelFilePath().AsUTF8Unsafe()
          << "\nHas metadata: "
          << (model_info.GetModelMetadata() ? "True" : "False");
    }
  }
}

void PredictionManager::OnPredictionModelsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::MaybeInitializeModelDownloads(
    download::BackgroundDownloadService* background_download_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (features::IsInstallWideModelStoreEnabled()) {
    init_time_ = base::TimeTicks::Now();
  }

  // Create the download manager here if we are allowed to.
  if (features::IsModelDownloadingEnabled() && !off_the_record_ &&
      !prediction_model_download_manager_) {
    prediction_model_download_manager_ =
        std::make_unique<PredictionModelDownloadManager>(
            background_download_service,
            base::BindRepeating(
                &PredictionManager::GetBaseModelDirForDownload,
                // base::Unretained is safe here because the
                // PredictionModelDownloadManager is owned by `this`
                base::Unretained(this)),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
    prediction_model_download_manager_->AddObserver(this);
  }

  // Only load models if there are optimization targets registered.
  if (!registered_optimization_targets_and_metadata_.empty() &&
      ShouldFetchModels(off_the_record_,
                        component_updates_enabled_provider_.Run())) {
    prediction_model_fetch_timer_.MaybeScheduleFirstModelFetch();
  }
}

void PredictionManager::OnStoreInitialized(
    BackgroundDownloadServiceProvider background_download_service_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!features::IsInstallWideModelStoreEnabled());

  store_is_ready_ = true;
  init_time_ = base::TimeTicks::Now();
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.StoreInitialized", true);

  // Purge any inactive models from the store.
  if (model_and_features_store_)
    model_and_features_store_->PurgeInactiveModels();

  MaybeInitializeModelDownloads(
      background_download_service_provider && !off_the_record_
          ? std::move(background_download_service_provider).Run()
          : nullptr);

  // Only load models if there are optimization targets registered.
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  // The store is ready so start loading models for the registered optimization
  // targets.
  LoadPredictionModels(GetRegisteredOptimizationTargets());
}

void PredictionManager::OnPredictionModelOverrideLoaded(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<proto::PredictionModel> prediction_model) {
  OnLoadPredictionModel(optimization_target,
                        /*record_availability_metrics=*/false,
                        std::move(prediction_model));
  RecordModelAvailableAtRegistration(optimization_target,
                                     prediction_model != nullptr);
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (switches::IsModelOverridePresent()) {
    for (proto::OptimizationTarget optimization_target : optimization_targets) {
      base::FilePath base_model_dir =
          GetBaseModelDirForDownload(optimization_target);
      BuildPredictionModelFromCommandLineForOptimizationTarget(
          optimization_target, base_model_dir,
          base::BindOnce(&PredictionManager::OnPredictionModelOverrideLoaded,
                         ui_weak_ptr_factory_.GetWeakPtr(),
                         optimization_target));
    }
    return;
  }

  if (features::IsInstallWideModelStoreEnabled()) {
    for (const auto optimization_target : optimization_targets) {
      if (!prediction_model_store_->HasModel(optimization_target,
                                             model_cache_key_)) {
        RecordModelAvailableAtRegistration(optimization_target, false);
        continue;
      }
      prediction_model_store_->LoadModel(
          optimization_target, model_cache_key_,
          base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                         ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                         /*record_availability_metrics=*/true));
    }
    return;
  }

  DCHECK(!features::IsInstallWideModelStoreEnabled());
  if (!model_and_features_store_)
    return;

  OptimizationGuideStore::EntryKey model_entry_key;
  for (const auto& optimization_target : optimization_targets) {
    // The prediction model for this optimization target has already been
    // loaded.
    bool model_stored_locally =
        model_and_features_store_->FindPredictionModelEntryKey(
            optimization_target, &model_entry_key);
    if (!model_stored_locally) {
      RecordModelAvailableAtRegistration(optimization_target,
                                         model_stored_locally);
      continue;
    }
    model_and_features_store_->LoadPredictionModel(
        model_entry_key,
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
  if (record_availability_metrics)
    RecordModelAvailableAtRegistration(optimization_target, success);
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
  if (features::IsInstallWideModelStoreEnabled()) {
    if (prediction_model_store_->HasModel(optimization_target,
                                          model_cache_key_)) {
      prediction_model_store_->RemoveModel(
          optimization_target, model_cache_key_, model_removal_reason);
    }
    return;
  }

  // Remove model from store if it exists.
  OptimizationGuideStore::EntryKey model_entry_key;
  if (model_and_features_store_ &&
      model_and_features_store_->FindPredictionModelEntryKey(
          optimization_target, &model_entry_key)) {
    base::UmaHistogramBoolean(
        "OptimizationGuide.PredictionModelRemoved." +
            GetStringNameForOptimizationTarget(optimization_target),
        true);
    model_and_features_store_->RemovePredictionModelFromEntryKey(
        model_entry_key);
  }
}

bool PredictionManager::ProcessAndStoreLoadedModel(
    const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.model_info().has_optimization_target())
    return false;
  if (!model.model_info().has_version())
    return false;
  if (!model.has_model())
    return false;
  if (!registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    return false;
  }

  ScopedPredictionModelConstructionAndValidationRecorder
      prediction_model_recorder(model.model_info().optimization_target());
  std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);
  if (!model_info) {
    prediction_model_recorder.set_is_valid(false);
    return false;
  }

  proto::OptimizationTarget optimization_target =
      model.model_info().optimization_target();

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

  auto model_meta_it =
      optimization_target_model_info_map_.find(optimization_target);
  if (model_meta_it != optimization_target_model_info_map_.end())
    return model_meta_it->second->GetVersion() != new_version;

  return true;
}

void PredictionManager::StoreLoadedModelInfo(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_info);

  // Notify observers of new model file path.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PredictionManager::NotifyObserversOfNewModel,
                                ui_weak_ptr_factory_.GetWeakPtr(),
                                optimization_target, *model_info));

  optimization_target_model_info_map_.insert_or_assign(optimization_target,
                                                       std::move(model_info));
}

void PredictionManager::SetClockForTesting(const base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clock_ = clock;
  prediction_model_fetch_timer_.SetClockForTesting(clock);  // IN-TEST
}

base::FilePath PredictionManager::GetBaseModelDirForDownload(
    proto::OptimizationTarget optimization_target) {
  return features::IsInstallWideModelStoreEnabled()
             ? prediction_model_store_->GetBaseModelDirForModelCacheKey(
                   optimization_target, model_cache_key_)
             : models_dir_path_.AppendASCII(
                   base::Uuid::GenerateRandomV4().AsLowercaseString());
}

void PredictionManager::OverrideTargetModelForTesting(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  if (!model_info) {
    return;
  }

  ModelInfo model_info_copy = *model_info;

  optimization_target_model_info_map_.insert_or_assign(optimization_target,
                                                       std::move(model_info));

  NotifyObserversOfNewModel(optimization_target, model_info_copy);
}

}  // namespace optimization_guide
