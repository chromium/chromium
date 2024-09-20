// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/optimization_guide/core/prediction_model_fetcher.h"
#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using leveldb_proto::test::FakeDB;

namespace {

// Retry delay is 2 minutes to allow for fetch retry delay + some random delay
// to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 2 + 62;
// 24 hours + random fetch delay.
constexpr int kUpdateFetchModelAndFeaturesTimeSecs = 24 * 60 * 60 + 62;

constexpr char kTestLocale[] = "en-US";

}  // namespace

namespace optimization_guide {

proto::PredictionModel CreatePredictionModelForGetModelsResponse(
    proto::OptimizationTarget optimization_target) {
  proto::PredictionModel prediction_model;

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->set_optimization_target(optimization_target);
  model_info->add_supported_model_engine_versions(
      proto::ModelEngineVersion::MODEL_ENGINE_VERSION_TFLITE_2_8);
  prediction_model.mutable_model()->set_download_url(
      "https://example.com/model");
  return prediction_model;
}

std::unique_ptr<proto::GetModelsResponse> BuildGetModelsResponse(
    std::set<proto::OptimizationTarget> optimization_targets) {
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      std::make_unique<proto::GetModelsResponse>();

  for (const auto& optimization_target : optimization_targets) {
    proto::PredictionModel prediction_model =
        CreatePredictionModelForGetModelsResponse(optimization_target);
    *get_models_response->add_models() = std::move(prediction_model);
  }

  return get_models_response;
}

proto::ModelCacheKey GetTestModelCacheKey() {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(kTestLocale);
  return model_cache_key;
}

class FakeOptimizationTargetModelObserver
    : public OptimizationTargetModelObserver {
 public:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      base::optional_ref<const ModelInfo> model_info) override {
    if (!model_info.has_value()) {
      last_received_models_.insert_or_assign(optimization_target, std::nullopt);
      return;
    }
    last_received_models_.insert_or_assign(optimization_target, *model_info);
  }

  bool WasNullModelReceived(proto::OptimizationTarget optimization_target) {
    auto model_it = last_received_models_.find(optimization_target);
    return (model_it != last_received_models_.end()) && !model_it->second;
  }

  // Returns the last received ModelInfo for |optimization_target|. Does not
  // differentiate whether a null model was received, or no model was received.
  std::optional<ModelInfo> last_received_model_for_target(
      proto::OptimizationTarget optimization_target) const {
    auto model_it = last_received_models_.find(optimization_target);
    if (model_it == last_received_models_.end())
      return std::nullopt;
    return model_it->second;
  }

  // Resets the state of the observer.
  void Reset() { last_received_models_.clear(); }

 private:
  // Contains the ModelInfo received per opt target. Three cases possible: Valid
  // ModelInfo received. Null ModelInfo received is indicated by the presence of
  // an entry with nullopt. No ModelInfo received is indicated by not having an
  // entry for the opt target.
  base::flat_map<proto::OptimizationTarget, std::optional<ModelInfo>>
      last_received_models_;
};

class FakePredictionModelDownloadManager
    : public PredictionModelDownloadManager {
 public:
  explicit FakePredictionModelDownloadManager(
      GetBaseModelDirForDownloadCallback
          get_base_model_dir_for_download_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : PredictionModelDownloadManager(
            /*download_service=*/nullptr,
            get_base_model_dir_for_download_callback,
            task_runner) {}
  ~FakePredictionModelDownloadManager() override = default;

  void StartDownload(const GURL& url,
                     proto::OptimizationTarget optimization_target) override {
    last_requested_download_ = url;
    last_requested_optimization_target_ = optimization_target;
  }

  GURL last_requested_download() const { return last_requested_download_; }

  proto::OptimizationTarget last_requested_optimization_target() const {
    return last_requested_optimization_target_;
  }

  void CancelAllPendingDownloads() override { cancel_downloads_called_ = true; }
  bool cancel_downloads_called() const { return cancel_downloads_called_; }

  bool IsAvailableForDownloads() const override { return is_available_; }
  void SetAvailableForDownloads(bool is_available) {
    is_available_ = is_available;
  }

 private:
  GURL last_requested_download_;
  proto::OptimizationTarget last_requested_optimization_target_;
  bool cancel_downloads_called_ = false;
  bool is_available_ = true;
};

enum class PredictionModelFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithModels = 1,
  kFetchSuccessWithEmptyResponse = 2,
};

void RunGetModelsCallback(
    ModelsFetchedCallback callback,
    std::unique_ptr<proto::GetModelsResponse> get_models_response) {
  if (get_models_response) {
    std::move(callback).Run(std::move(get_models_response));
    return;
  }
  std::move(callback).Run(std::nullopt);
}

// A mock class implementation of PredictionModelFetcherImpl.
class TestPredictionModelFetcher : public PredictionModelFetcherImpl {
 public:
  TestPredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_get_models_url,
      PredictionModelFetcherEndState fetch_state)
      : PredictionModelFetcherImpl(url_loader_factory,
                                   optimization_guide_service_get_models_url),
        fetch_state_(fetch_state) {
    success_fetch_optimization_targets_.emplace(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  }

  bool FetchOptimizationGuideServiceModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      proto::RequestContext request_context,
      const std::string& locale,
      ModelsFetchedCallback models_fetched_callback) override {
    if (!ValidateModelsInfoForFetch(models_request_info)) {
      std::move(models_fetched_callback).Run(std::nullopt);
      return false;
    }

    std::unique_ptr<proto::GetModelsResponse> get_models_response;
    locale_requested_ = locale;
    switch (fetch_state_) {
      case PredictionModelFetcherEndState::kFetchFailed:
        get_models_response = nullptr;
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithModels:
        models_fetched_ = true;
        get_models_response =
            BuildGetModelsResponse(success_fetch_optimization_targets_);
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse:
        models_fetched_ = true;
        get_models_response = std::make_unique<proto::GetModelsResponse>();
        break;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RunGetModelsCallback,
                                  std::move(models_fetched_callback),
                                  std::move(get_models_response)));
    return true;
  }

  bool ValidateModelsInfoForFetch(
      const std::vector<proto::ModelInfo>& models_request_info) {
    for (const auto& model_info : models_request_info) {
      if (model_info.supported_model_engine_versions_size() == 0 ||
          !proto::ModelEngineVersion_IsValid(
              model_info.supported_model_engine_versions(0))) {
        return false;
      }
      if (!model_info.has_optimization_target() ||
          !proto::OptimizationTarget_IsValid(
              model_info.optimization_target())) {
        return false;
      }

      if (check_expected_version_) {
        auto version_it =
            expected_version_.find(model_info.optimization_target());
        if (model_info.has_version() !=
            (version_it != expected_version_.end())) {
          return false;
        }
        if (model_info.has_version() &&
            model_info.version() != version_it->second) {
          return false;
        }
      }

      auto it = expected_metadata_.find(model_info.optimization_target());
      if (model_info.has_model_metadata() != (it != expected_metadata_.end()))
        return false;
      if (model_info.has_model_metadata()) {
        proto::Any expected_metadata = it->second;
        if (model_info.model_metadata().type_url() !=
            expected_metadata.type_url()) {
          return false;
        }
        if (model_info.model_metadata().value() != expected_metadata.value())
          return false;
      }
    }
    return true;
  }

  void SetExpectedModelMetadataForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      const proto::Any& model_metadata) {
    expected_metadata_[optimization_target] = model_metadata;
  }

  void SetExpectedVersionForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      int64_t version) {
    expected_version_[optimization_target] = version;
  }

  void SetCheckExpectedVersion() { check_expected_version_ = true; }

  void AddOptimizationTargetToSuccessFetch(
      proto::OptimizationTarget optimization_target) {
    success_fetch_optimization_targets_.emplace(optimization_target);
  }

  void Reset() {
    models_fetched_ = false;
    success_fetch_optimization_targets_.clear();
  }

  bool models_fetched() const { return models_fetched_; }

  std::string locale_requested() const { return locale_requested_; }

 private:
  bool models_fetched_ = false;
  bool check_expected_version_ = false;
  std::string locale_requested_;
  PredictionModelFetcherEndState fetch_state_;

  // Optimization targets for which models should be returned, in case of a
  // successful fetch.
  std::set<proto::OptimizationTarget> success_fetch_optimization_targets_;

  // The desired behavior of the TestPredictionModelFetcher.
  base::flat_map<proto::OptimizationTarget, proto::Any> expected_metadata_;
  base::flat_map<proto::OptimizationTarget, int64_t> expected_version_;
};


class TestPredictionManager : public PredictionManager {
 public:
  TestPredictionManager(
      PredictionModelStore* prediction_model_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      ComponentUpdatesEnabledProvider component_updates_enabled_provider,
      bool off_the_record,
      const std::string& application_locale,
      const base::FilePath& models_dir_path)
      : PredictionManager(
            prediction_model_store,
            url_loader_factory,
            pref_service,
            off_the_record,
            application_locale,
            models_dir_path,
            &optimization_guide_logger_,
            /*background_download_service_provider=*/
            base::OnceCallback<download::BackgroundDownloadService*()>(),
            component_updates_enabled_provider) {}

  ~TestPredictionManager() override = default;

 private:
  OptimizationGuideLogger optimization_guide_logger_;
};

class TestPredictionModelStore : public PredictionModelStore {
 public:
  explicit TestPredictionModelStore(PrefService* local_state)
      : local_state_(local_state) {}

  // PredictionModelStore:
  PrefService* GetLocalState() const override { return local_state_; }

 private:
  raw_ptr<PrefService> local_state_;
};

class PredictionManagerTestBase : public ProtoDatabaseProviderTestBase {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<OptimizationGuideStore::EntryKey, StoreEntry>;
  PredictionManagerTestBase() = default;
  ~PredictionManagerTestBase() override = default;

  PredictionManagerTestBase(const PredictionManagerTestBase&) = delete;
  PredictionManagerTestBase& operator=(const PredictionManagerTestBase&) =
      delete;

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();

    local_state_prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterLocalStatePrefs(local_state_prefs_->registry());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kGoogleApiKeyConfigurationCheckOverride);

    CreateAndInitializePredictionModelStore();
    RunUntilIdle();
  }

  void CreatePredictionManager() {
    if (prediction_manager_) {
      db_store_.clear();
      prediction_manager_.reset();
    }

    prediction_manager_ = std::make_unique<TestPredictionManager>(
        prediction_model_store_.get(), url_loader_factory_, pref_service_.get(),
        base::BindRepeating(
            &PredictionManagerTestBase::AreComponentUpdatesEnabled,
            base::Unretained(this)),
        false, kTestLocale, temp_dir());
    prediction_manager_->GetPredictionModelFetchTimerForTesting()
        ->SetClockForTesting(task_environment_.GetMockClock());
  }

  void CreateAndInitializePredictionModelStore() {
    prediction_model_store_ =
        std::make_unique<TestPredictionModelStore>(local_state_prefs_.get());
    prediction_model_store_->Initialize(temp_dir());
  }

  TestPredictionManager* prediction_manager() const {
    return prediction_manager_.get();
  }

  void TearDown() override { ProtoDatabaseProviderTestBase::TearDown(); }

  std::unique_ptr<TestPredictionModelFetcher> BuildTestPredictionModelFetcher(
      PredictionModelFetcherEndState end_state) {
    std::unique_ptr<TestPredictionModelFetcher> prediction_model_fetcher =
        std::make_unique<TestPredictionModelFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"), end_state);
    return prediction_model_fetcher;
  }

  void SetStoreInitialized(bool load_models = true,
                           bool have_models_in_store = true) {
    prediction_manager_->MaybeInitializeModelDownloads(
        /*background_download_service=*/nullptr);
    RunUntilIdle();
    // Move clock forward for any short delays added for the fetcher, until the
    // startup fetch could start.
    MoveClockForwardBy(base::Seconds(12));
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  TestPredictionModelFetcher* prediction_model_fetcher() const {
    return static_cast<TestPredictionModelFetcher*>(
        prediction_manager()->prediction_model_fetcher());
  }

  void CreatePredictionModelDownloadManager() {
    prediction_manager()->SetPredictionModelDownloadManagerForTesting(
        std::make_unique<FakePredictionModelDownloadManager>(
            base::BindRepeating(&PredictionManager::GetBaseModelDirForDownload,
                                base::Unretained(prediction_manager())),
            task_environment()->GetMainThreadTaskRunner()));
  }

  FakePredictionModelDownloadManager* prediction_model_download_manager()
      const {
    return static_cast<FakePredictionModelDownloadManager*>(
        base::BindRepeating(&PredictionManager::GetBaseModelDirForDownload,
                            base::Unretained(prediction_manager())),
        prediction_manager()->prediction_model_download_manager());
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  PredictionModelStore* prediction_model_store() {
    return prediction_model_store_.get();
  }

  void SetComponentUpdatesPrefEnabled(bool enabled) {
    component_updates_enabled_ = enabled;
  }

  bool AreComponentUpdatesEnabled() const { return component_updates_enabled_; }

 protected:
  // |feature_list_| needs to be destroyed after |task_environment_|, to avoid
  // tsan flakes caused by other tasks running while |feature_list_| is
  // destroyed.
  base::test::ScopedFeatureList feature_list_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  StoreEntryMap db_store_;
  std::unique_ptr<TestPredictionModelStore> prediction_model_store_;
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  bool component_updates_enabled_ = true;
};

class PredictionManagerRemoteFetchingDisabledTest
    : public PredictionManagerTestBase {
 public:
  PredictionManagerRemoteFetchingDisabledTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitWithFeatures(
        {}, {features::kRemoteOptimizationGuideFetching});
  }
};

TEST_F(PredictionManagerRemoteFetchingDisabledTest, RemoteFetchingDisabled) {
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

class PredictionManagerModelDownloadingDisabledTest
    : public PredictionManagerTestBase {
 public:
  PredictionManagerModelDownloadingDisabledTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitWithFeatures(
        {}, {features::kOptimizationGuideModelDownloading});
  }
};

TEST_F(PredictionManagerModelDownloadingDisabledTest,
       ModelDownloadingDisabledShouldNotFetch) {
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

class PredictionManagerTest : public PredictionManagerTestBase {
 public:
  PredictionManagerTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kRemoteOptimizationGuideFetching,
        features::kOptimizationGuideModelDownloading,
    };
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  proto::PredictionModel CreatePredictionModelForModelStore(
      proto::OptimizationTarget optimization_target) {
    auto base_model_dir = GetBaseModelDir(optimization_target);
    proto::PredictionModel prediction_model;
    proto::ModelInfo* model_info = prediction_model.mutable_model_info();
    model_info->set_optimization_target(optimization_target);
    model_info->set_version(1);
    model_info->add_supported_model_engine_versions(
        proto::ModelEngineVersion::MODEL_ENGINE_VERSION_TFLITE_2_8);
    prediction_model.mutable_model()->set_download_url(
        FilePathToString(base_model_dir.Append(GetBaseFileNameForModels())));
    CreateTestModelFiles(*model_info, base_model_dir);
    return prediction_model;
  }

  void CreateTestModelFiles(const proto::ModelInfo& model_info,
                            const base::FilePath& base_model_dir) {
    CreateDirectory(base_model_dir);
    WriteFile(base_model_dir.Append(GetBaseFileNameForModels()), "");
    std::string model_info_str;
    ASSERT_TRUE(model_info.SerializeToString(&model_info_str));
    WriteFile(base_model_dir.Append(GetBaseFileNameForModelInfo()),
              model_info_str);
    RunUntilIdle();
  }

  base::FilePath GetBaseModelDir(
      proto::OptimizationTarget optimization_target) const {
    return temp_dir().AppendASCII(
        proto::OptimizationTarget_Name(optimization_target));
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

TEST_F(PredictionManagerTest, RemoteFetchingPrefDisabled) {
  SetComponentUpdatesPrefEnabled(false);
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, RemoteFetchingPrefEnabledAndThenDisabled) {
  SetComponentUpdatesPrefEnabled(true);
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  model.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);
  RunUntilIdle();
  EXPECT_TRUE(observer.last_received_model_for_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  prediction_model_fetcher()->Reset();
  observer.Reset();

  // No fetch should happen with the pref disabled. But model is still
  // delivered.
  SetComponentUpdatesPrefEnabled(false);
  prediction_manager()->RemoveObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &observer);
  MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  RunUntilIdle();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  EXPECT_TRUE(observer.last_received_model_for_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  prediction_model_fetcher()->Reset();
  observer.Reset();
}

TEST_F(PredictionManagerTest, AddObserverForOptimizationTargetModel) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit."
      "PainfulPageLoad",
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.FirstModelFetchSinceServiceInit", 0);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit."
      "PainfulPageLoad",
      1);

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  // Make sure the test histogram is recorded. We don't check for value here
  // since that is too much toil for someone whenever they add a new version.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.FirstModelFetchSinceServiceInit", 1);

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());

  // Ensure observer is hooked up.
  {
    base::HistogramTester model_ready_histogram_tester;
    auto base_model_dir =
        GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    proto::PredictionModel model1 = CreatePredictionModelForModelStore(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    auto additional_file_path =
        base_model_dir.AppendASCII("additional_file.txt");
    model1.mutable_model_info()->add_additional_files()->set_file_path(
        FilePathToString(additional_file_path));
    // An empty file path should be be ignored.
    model1.mutable_model_info()->add_additional_files()->set_file_path("");
    model1.mutable_model_info()->mutable_model_metadata()->set_type_url(
        "sometypeurl");

    prediction_manager()->OnModelReady(base_model_dir, model1);
    RunUntilIdle();

    std::optional<ModelInfo> received_model =
        observer.last_received_model_for_target(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_EQ(received_model->GetModelMetadata()->type_url(), "sometypeurl");
    EXPECT_EQ(base_model_dir.Append(GetBaseFileNameForModels()),
              received_model->GetModelFilePath());
    auto additional_file = received_model->GetAdditionalFileWithBaseName(
        base::FilePath::StringType(FILE_PATH_LITERAL("additional_file.txt")));
    ASSERT_TRUE(additional_file);
    EXPECT_EQ(*additional_file, additional_file_path);

    // Make sure we do not record the model available histogram again.
    model_ready_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
        "PainfulPageLoad",
        0);
  }

  // Reset fetcher and make sure version is sent in the new request and not
  // counted as re-loaded or updated.
  {
    base::HistogramTester histogram_tester2;

    prediction_model_fetcher()->Reset();
    prediction_model_fetcher()->SetCheckExpectedVersion();
    prediction_model_fetcher()->SetExpectedVersionForOptimizationTarget(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 1);
    MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
    EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", 0);
  }

  // Now remove and reset observer.
  prediction_manager()->RemoveObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &observer);
  observer.Reset();
  auto base_model_dir = temp_dir().AppendASCII("bar");
  proto::PredictionModel model2 = CreatePredictionModelForModelStore(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model2.mutable_model_info()->set_version(2);
  model2.mutable_model()->set_download_url(
      FilePathToString(base_model_dir.AppendASCII("bar_model.tflite")));
  model2.mutable_model_info()->mutable_model_metadata()->set_type_url(
      "sometypeurl");
  prediction_manager()->OnModelReady(base_model_dir, model2);
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // removed.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_F(PredictionManagerTest,
       AddObserverForOptimizationTargetModelAddAnotherObserverForSameTarget) {
  // Fails under "threadsafe" mode.
  GTEST_FLAG_SET(death_test_style, "fast");

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer1;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt, &observer1);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  // Ensure observer is hooked up.
  auto base_model_dir =
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  prediction_manager()->OnModelReady(
      base_model_dir, CreatePredictionModelForModelStore(
                          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  RunUntilIdle();

  EXPECT_EQ(base_model_dir.Append(GetBaseFileNameForModels()),
            observer1
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->GetModelFilePath());

#if !BUILDFLAG(IS_WIN)
  // Do not run the DCHECK death test on Windows since there's some weird
  // behavior there.

  // Now, register a new observer - it should die.
  FakeOptimizationTargetModelObserver observer2;
  EXPECT_DCHECK_DEATH(
      prediction_manager()->AddObserverForOptimizationTargetModel(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          /*model_metadata=*/std::nullopt, &observer2));
  RunUntilIdle();
#endif
}

// See crbug/1227996.
#if !BUILDFLAG(IS_WIN)
TEST_F(PredictionManagerTest,
       AddObserverForOptimizationTargetModelCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Any metadata;
  metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  encoded_metadata = base::Base64Encode(encoded_metadata);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  // Make sure no models are fetched.
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  // However, expect that the histogram for model engine version is recorded.
  // We don't check for value here since that is too much toil for someone
  // whenever they add a new version.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion", 1);

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(
      observer
          .last_received_model_for_target(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          ->GetModelMetadata()
          ->type_url(),
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata");
  EXPECT_EQ(observer
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->GetModelFilePath()
                .value(),
            FILE_PATH_LITERAL(kTestAbsoluteFilePath));

  // Now reset observer. New model downloads should not update the observer.
  observer.Reset();
  prediction_manager()->OnModelReady(
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      CreatePredictionModelForModelStore(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // reset and override is in place.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}
#endif

TEST_F(PredictionManagerTest,
       NoPredictionModelForRegisteredOptimizationTarget) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  SetStoreInitialized(/*load_models=*/false, /*have_models_in_store=*/false);
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, std::nullopt, &observer);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "ModelValidation",
      false, 1);
}

// Tests the opt target observer valid re-registrations, i.e., adding, removing
// and then re-adding an observer for the same opt target.
TEST_F(PredictionManagerTest, OptimizationTargetModelObserverReRegistrations) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  model.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);
  RunUntilIdle();
  EXPECT_TRUE(observer.last_received_model_for_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  prediction_model_fetcher()->Reset();
  observer.Reset();

  // Re-registering should also deliver the model.
  prediction_manager()->RemoveObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &observer);
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  EXPECT_TRUE(observer.last_received_model_for_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  // Model is delivered from store. No fetch happens.
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, UpdatePredictionModelsWithInvalidModel) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt, &observer);

  // Set invalid model with no download url.
  proto::PredictionModel model = CreatePredictionModelForModelStore(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model()->clear_download_url();
  prediction_manager()->OnModelReady(
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD), model);
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.IsPredictionModelValid.PainfulPageLoad", false, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest, UpdateModelFileWithSameVersion) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt, &observer);

  auto base_model_dir =
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  proto::PredictionModel model = CreatePredictionModelForModelStore(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  prediction_manager()->OnModelReady(base_model_dir, model);
  RunUntilIdle();

  EXPECT_TRUE(observer
                  .last_received_model_for_target(
                      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                  .has_value());

  // Now reset the observer state.
  observer.Reset();

  // Send the same model again.
  prediction_manager()->OnModelReady(base_model_dir, model);

  // The observer should not have received an update.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_F(PredictionManagerTest, DownloadManagerUnavailableShouldNotFetch) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();
  prediction_model_download_manager()->SetAvailableForDownloads(false);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  SetStoreInitialized(/*load_models=*/true, /*have_models_in_store=*/false);
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kDownloadServiceUnavailable, 1);
}

TEST_F(PredictionManagerTest, UpdateModelWithDownloadUrl) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_TRUE(prediction_model_download_manager()->cancel_downloads_called());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.IsDownloadUrlValid.PainfulPageLoad",
      true, 1);

  EXPECT_EQ(prediction_model_download_manager()->last_requested_download(),
            GURL("https://example.com/model"));
  EXPECT_EQ(
      prediction_model_download_manager()->last_requested_optimization_target(),
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
}

TEST_F(PredictionManagerTest, UpdateModelForUnregisteredTargetOnModelReady) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();

  SetStoreInitialized();

  prediction_manager()->OnModelReady(
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      CreatePredictionModelForModelStore(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);

  // Now register the model.
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kModelDownloaded, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kModelDelivered, 1);
}

TEST_F(PredictionManagerTest,
       StoreInitializedAfterOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
    auto model = CreatePredictionModelForModelStore(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    prediction_model_store()->UpdateModel(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey(),
        model.model_info(),
        GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
        base::DoNothing());
    RunUntilIdle();

  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
}

TEST_F(PredictionManagerTest,
       StoreInitializedBeforeOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
    auto model = CreatePredictionModelForModelStore(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    prediction_model_store()->UpdateModel(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey(),
        model.model_info(),
        GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
        base::DoNothing());
    RunUntilIdle();

  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  SetStoreInitialized();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);
  RunUntilIdle();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
}

TEST_F(PredictionManagerTest, ModelFetcherTimerRetryDelay) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, ModelFetcherTimerFetchSucceeds) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_EQ("en-US", prediction_model_fetcher()->locale_requested());

  // Reset the prediction model fetcher to detect when the next fetch occurs.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, ModelRemovedWhenMissingInGetModelsResponse) {
  FakeOptimizationTargetModelObserver observer;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();
  prediction_model_fetcher()->AddOptimizationTargetToSuccessFetch(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, std::nullopt, &observer);

  // Load the model and let it be saved in the store.
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ true);
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  prediction_manager()->OnModelReady(
      GetBaseModelDir(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      CreatePredictionModelForModelStore(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  RunUntilIdle();
  EXPECT_TRUE(prediction_model_store()->HasModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey()));
  EXPECT_TRUE(observer.last_received_model_for_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  {
    // Let no model be sent in GetModelsResponse, and that triggers the model
    // to be removed from the store.
    base::HistogramTester histogram_tester;
    prediction_model_fetcher()->Reset();
    observer.Reset();
    MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
    EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
        PredictionModelStoreModelRemovalReason::kNoModelInGetModelsResponse, 1);
    EXPECT_FALSE(prediction_model_store()->HasModel(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey()));
    EXPECT_TRUE(observer.WasNullModelReceived(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  }
}

}  // namespace optimization_guide
