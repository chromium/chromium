// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/prediction_model_download_observer.h"
#include "components/optimization_guide/core/prediction_model_fetch_timer.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/origin.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class OptimizationGuideLogger;
class PrefService;

namespace optimization_guide {

class OptimizationTargetModelObserver;
class PredictionModelDownloadManager;
class PredictionModelFetcher;
class PredictionModelStore;
class ModelInfo;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager : public PredictionModelDownloadObserver {
 public:
  // BackgroundDownloadService is only available once the profile is fully
  // initialized and that cannot be done as part of |Initialize|. Get a provider
  // to retrieve the service when it is needed.
  using BackgroundDownloadServiceProvider =
      base::OnceCallback<download::BackgroundDownloadService*(void)>;

  // Callback to whether component updates are enabled for the browser.
  using ComponentUpdatesEnabledProvider = base::RepeatingCallback<bool(void)>;

  PredictionManager(
      PredictionModelStore* prediction_model_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      bool off_the_record,
      const std::string& application_locale,
      const base::FilePath& models_dir_path,
      OptimizationGuideLogger* optimization_guide_logger,
      BackgroundDownloadServiceProvider background_download_service_provider,
      ComponentUpdatesEnabledProvider component_updates_enabled_provider);

  PredictionManager(const PredictionManager&) = delete;
  PredictionManager& operator=(const PredictionManager&) = delete;

  ~PredictionManager() override;

  // Adds an observer for updates to the model for |optimization_target|.
  //
  // It is assumed that any model retrieved this way will be passed to the
  // Machine Learning Service for inference.
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer);

  // Removes an observer for updates to the model for |optimization_target|.
  //
  // If |observer| is registered for multiple targets, |observer| must be
  // removed for all observed targets for in order for it to be fully
  // removed from receiving any calls.
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer);

  // Set the prediction model fetcher for testing.
  void SetPredictionModelFetcherForTesting(
      std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher);

  PredictionModelFetcher* prediction_model_fetcher() const {
    return prediction_model_fetcher_.get();
  }

  // Set the prediction model download manager for testing.
  void SetPredictionModelDownloadManagerForTesting(
      std::unique_ptr<PredictionModelDownloadManager>
          prediction_model_download_manager);

  PredictionModelDownloadManager* prediction_model_download_manager() const {
    return prediction_model_download_manager_.get();
  }

  // Return the optimization targets that are registered.
  base::flat_set<proto::OptimizationTarget> GetRegisteredOptimizationTargets()
      const;

  // Override the model file returned to observers for |optimization_target|.
  // Use |TestModelInfoBuilder| to construct the model files. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<ModelInfo> model_info);

  // PredictionModelDownloadObserver:
  void OnModelReady(const base::FilePath& base_model_dir,
                    const proto::PredictionModel& model) override;
  void OnModelDownloadStarted(
      proto::OptimizationTarget optimization_target) override;
  void OnModelDownloadFailed(
      proto::OptimizationTarget optimization_target) override;

  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
  GetDownloadedModelsInfoForWebUI() const;

  // Initialize the model metadata fetching and downloads.
  void MaybeInitializeModelDownloads(
      download::BackgroundDownloadService* background_download_service);

  PredictionModelFetchTimer* GetPredictionModelFetchTimerForTesting() {
    return &prediction_model_fetch_timer_;
  }

 protected:
  // Process `prediction_models` to be stored in the in memory optimization
  // target prediction model map for immediate use and asynchronously write the
  // models to the model and features store to be persisted.
  // `models_request_info` is the list of models the fetch request was made
  // for, and `prediction_models` is the models received in response. Any models
  // missing in the response will be deleted from the store, since the remote
  // optimization guide service has no models for them.
  void UpdatePredictionModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
          prediction_models);

 private:
  // Contains the model registration specific info to be kept for each
  // optimization target.
  struct ModelRegistrationInfo {
    explicit ModelRegistrationInfo(std::optional<proto::Any> metadata);
    ~ModelRegistrationInfo();

    // The feature-provided metadata that was registered with the prediction
    // manager.
    std::optional<proto::Any> metadata;

    // The set of model observers that were registered to receive model updates
    // from the prediction manager.
    base::ObserverList<OptimizationTargetModelObserver> model_observers;
  };

  friend class PredictionManagerTestBase;
  friend class PredictionModelStoreBrowserTestBase;

  // Called on construction to initialize the prediction model.
  // |background_dowload_service_provider| can provide the
  // BackgroundDownloadService if needed to download models.
  void Initialize(
      BackgroundDownloadServiceProvider background_dowload_service_provider);

  // Called to make a request to fetch models from the remote Optimization Guide
  // Service. Used to fetch models for the registered optimization targets.
  void FetchModels();

  // Callback when the models have been fetched from the remote Optimization
  // Guide Service and are ready for parsing. Processes the prediction models in
  // the response and stores them for use. The metadata entry containing the
  // time that updates should be fetched from the remote Optimization Guide
  // Service is updated, even when the response is empty.
  void OnModelsFetched(const std::vector<proto::ModelInfo> models_request_info,
                       std::optional<std::unique_ptr<proto::GetModelsResponse>>
                           get_models_response_data);

  // Load models for every target in |optimization_targets| that have not yet
  // been loaded from the store.
  void LoadPredictionModels(
      const base::flat_set<proto::OptimizationTarget>& optimization_targets);

  // Callback run after prediction models are stored in
  // `prediction_model_store_`.
  void OnPredictionModelsStored();

  // Callback run after a prediction model is loaded from the store.
  // |prediction_model| is used to construct a PredictionModel capable of making
  // prediction for the appropriate |optimization_target|.
  void OnLoadPredictionModel(
      proto::OptimizationTarget optimization_target,
      bool record_availability_metrics,
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Callback run after a prediction model is loaded from a command-line
  // override.
  void OnPredictionModelOverrideLoaded(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Process loaded |model| into memory. Return true if a prediction
  // model object was created and successfully stored, otherwise false.
  bool ProcessAndStoreLoadedModel(const proto::PredictionModel& model);

  // Removes the model for `optimization_target` from store, for the
  // `model_removal_reason`.
  void RemoveModelFromStore(
      proto::OptimizationTarget optimization_target,
      PredictionModelStoreModelRemovalReason model_removal_reason);

  // Return whether the model stored in memory for |optimization_target| should
  // be updated based on what's currently stored and |new_version|.
  bool ShouldUpdateStoredModelForTarget(
      proto::OptimizationTarget optimization_target,
      int64_t new_version) const;

  // Updates the in-memory model file for |optimization_target| to
  // |prediction_model_file|.
  void StoreLoadedModelInfo(proto::OptimizationTarget optimization_target,
                            std::unique_ptr<ModelInfo> prediction_model_file);

  // Post-processing callback invoked after processing |model|.
  void OnProcessLoadedModel(const proto::PredictionModel& model, bool success);

  // Return the time when a prediction model fetch was last attempted.
  base::Time GetLastFetchAttemptTime() const;

  // Set the last time when a prediction model fetch was last attempted to
  // |last_attempt_time|.
  void SetLastModelFetchAttemptTime(base::Time last_attempt_time);

  // Return the time when a prediction model fetch was last successfully
  // completed.
  base::Time GetLastFetchSuccessTime() const;

  // Set the last time when a fetch for prediction models last succeeded to
  // |last_success_time|.
  void SetLastModelFetchSuccessTime(base::Time last_success_time);

  // Schedule first fetch for models if enabled for this profile.
  void MaybeScheduleFirstModelFetch();

  // Schedule |fetch_timer_| to fire based on:
  // 1. The update time for models in the store and
  // 2. The last time a fetch attempt was made.
  void ScheduleModelsFetch();

  // Notifies observers of `optimization_target` that the model has been
  // updated. `model_info` will be nullopt when the model was stopped to be
  // served from the server, and removed from the store,
  void NotifyObserversOfNewModel(
      proto::OptimizationTarget optimization_target,
      base::optional_ref<const ModelInfo> model_info);

  // Updates the metadata for |model|.
  void UpdateModelMetadata(const proto::PredictionModel& model);

  // Returns whether the model should be downloaded, or the correct model
  // version already exists in the model store.
  bool ShouldDownloadNewModel(const proto::PredictionModel& model) const;

  // Starts the model download for |optimization_target| from |download_url|.
  void StartModelDownload(proto::OptimizationTarget optimization_target,
                          const GURL& download_url);

  // Start downloading the model if the load failed, or update the model if it
  // is loaded fine.
  void MaybeDownloadOrUpdatePredictionModel(
      proto::OptimizationTarget optimization_target,
      const proto::PredictionModel& get_models_response_model,
      std::unique_ptr<proto::PredictionModel> loaded_model);

  // Returns a new file path for the directory to download the model files for
  // |optimization_target|. The directory will not be created.
  base::FilePath GetBaseModelDirForDownload(
      proto::OptimizationTarget optimization_target);

  void SetModelCacheKeyForTesting(const proto::ModelCacheKey& model_cache_key) {
    model_cache_key_ = model_cache_key;
  }

  // A map of optimization target to the model file containing the model for the
  // target.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<ModelInfo>>
      optimization_target_model_info_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The map from optimization target to the model registration specific data.
  std::map<proto::OptimizationTarget, ModelRegistrationInfo>
      model_registration_info_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The fetcher that handles making requests to update the models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher_;

  // The downloader that handles making requests to download the prediction
  // models. Can be null if model downloading is disabled.
  std::unique_ptr<PredictionModelDownloadManager>
      prediction_model_download_manager_;

  // The new optimization guide model store. Will be null when the feature is
  // not enabled. Not owned and outlives |this| since its an install-wide store.
  raw_ptr<PredictionModelStore> prediction_model_store_;

  // A stored response from a model and host model features fetch used to hold
  // models to be stored once host model features are processed and stored.
  std::unique_ptr<proto::GetModelsResponse> get_models_response_data_to_store_;

  // The URL loader factory used for fetching model and host feature updates
  // from the remote Optimization Guide Service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The repeating callback that will be used to determine if component updates
  // are enabled.
  ComponentUpdatesEnabledProvider component_updates_enabled_provider_;

  // Time the prediction manager got initialized.
  // TODO(crbug.com/40861855): Remove this old model store once the new model
  // store is launched.
  base::TimeTicks init_time_;

  PredictionModelFetchTimer prediction_model_fetch_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether the profile for this PredictionManager is off the record.
  bool off_the_record_ = false;

  // The locale of the application.
  std::string application_locale_;

  // Model cache key for the profile.
  proto::ModelCacheKey model_cache_key_;

  // The path to the directory containing the models.
  base::FilePath models_dir_path_;

  // Whether to check for Google API key configuration.
  bool should_check_google_api_key_configuration_ = true;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MANAGER_H_
