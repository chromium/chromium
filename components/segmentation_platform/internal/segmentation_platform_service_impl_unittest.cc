// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_test_base.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace segmentation_platform {
namespace {

const int64_t kModelVersion = 123;

// A mock of the ServiceProxy::Observer.
class MockServiceProxyObserver : public ServiceProxy::Observer {
 public:
  MockServiceProxyObserver() = default;
  ~MockServiceProxyObserver() override = default;

  MOCK_METHOD(void, OnServiceStatusChanged, (bool, int), (override));
  MOCK_METHOD(void,
              OnClientInfoAvailable,
              (const std::vector<ServiceProxy::ClientInfo>& client_info),
              (override));
};

}  // namespace

class SegmentationPlatformServiceImplTest
    : public testing::Test,
      public SegmentationPlatformServiceTestBase {
 public:
  explicit SegmentationPlatformServiceImplTest(
      std::unique_ptr<UkmDataManager> ukm_data_manager = nullptr) {
    feature_list_.InitAndEnableFeature(
        features::kSegmentationPlatformSignalDbCache);
    if (ukm_data_manager) {
      ukm_data_manager_ = std::move(ukm_data_manager);
      return;
    }
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    LocalStateHelper::GetInstance().Initialize(&prefs_);
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_recorder_.get());
    auto ukm_database = std::make_unique<NiceMock<MockUkmDatabase>>();
    ukm_data_manager_ = std::make_unique<UkmDataManagerImpl>();
    static_cast<UkmDataManagerImpl*>(ukm_data_manager_.get())
        ->InitializeForTesting(std::move(ukm_database), ukm_observer_.get());
  }

  ~SegmentationPlatformServiceImplTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    // Setup model provider data for default model for supporting on demand
    // execution.
    model_provider_data_.segments_supporting_default_model = {
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER};

    // TODO(ssid): use mock a history service here.
    SegmentationPlatformServiceTestBase::InitPlatform(
        ukm_data_manager_.get(), /*history_service=*/nullptr);

    segmentation_platform_service_impl_->GetServiceProxy()->AddObserver(
        &observer_);
  }

  void TearDown() override {
    SegmentationPlatformServiceTestBase::DestroyPlatform();
  }

  void SetUpDefaultModelProviders() {
    proto::SegmentationModelMetadata metadata;
    metadata.set_time_unit(proto::TimeUnit::DAY);
    metadata.set_bucket_duration(42u);
    auto& model_provider =
        *(*model_provider_data_.default_model_providers.find(
              SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER))
             .second;

    model_provider_data_.default_provider_metadata
        [SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER] = metadata;
    EXPECT_CALL(model_provider, ExecuteModelWithInput(_, _))
        .WillRepeatedly(
            RunOnceCallbackRepeatedly<1>(ModelProvider::Response(1, 1)));
  }

  void OnGetSelectedSegment(base::RepeatingClosure closure,
                            const SegmentSelectionResult& expected,
                            const SegmentSelectionResult& actual) {
    ASSERT_EQ(expected, actual);
    std::move(closure).Run();
  }

  void AssertSelectedSegment(
      const std::string& segmentation_key,
      bool is_ready,
      SegmentId expected = SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    SegmentSelectionResult result;
    result.is_ready = is_ready;
    if (is_ready)
      result.segment = expected;
    base::RunLoop loop;
    segmentation_platform_service_impl_->GetSelectedSegment(
        segmentation_key,
        base::BindOnce(
            &SegmentationPlatformServiceImplTest::OnGetSelectedSegment,
            base::Unretained(this), loop.QuitClosure(), result));
    loop.Run();
  }

  void AssertCachedSegment(
      const std::string& segmentation_key,
      bool is_ready,
      SegmentId expected = SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    SegmentSelectionResult result;
    result.is_ready = is_ready;
    if (is_ready)
      result.segment = expected;
    ASSERT_EQ(result,
              segmentation_platform_service_impl_->GetCachedSegmentResult(
                  segmentation_key));
  }

 protected:
  void TestInitializationFlow() {
    // ServiceProxy will load new segment infos from the DB.
    EXPECT_CALL(observer_, OnClientInfoAvailable(_)).Times(5);

    // Let the DB loading complete successfully.
    EXPECT_CALL(observer_, OnServiceStatusChanged(true, 7));
    segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    segment_storage_config_db_->InitStatusCallback(
        leveldb_proto::Enums::InitStatus::kOK);
    signal_db_->LoadCallback(true);
    segment_storage_config_db_->LoadCallback(true);

    // If initialization is succeeded, model execution scheduler should start
    // querying segment db.
    segment_db_->LoadCallback(true);

    // If we build with TF Lite, we need to also inspect whether the
    // ModelManagerImpl is publishing the correct data and whether that
    // leads to the SegmentationPlatformServiceImpl doing the right thing.
    base::HistogramTester histogram_tester;
    proto::SegmentationModelMetadata metadata;
    metadata.set_time_unit(proto::TimeUnit::DAY);
    metadata.set_bucket_duration(42u);
    // Add a test feature, which will later cause the signal storage DB to be
    // updated.
    auto* feature = metadata.add_features();
    feature->set_type(proto::SignalType::HISTOGRAM_VALUE);
    feature->set_name("other");
    feature->set_name_hash(base::HashMetricName("other"));
    feature->set_aggregation(proto::Aggregation::BUCKETED_SUM);
    feature->set_bucket_count(3);
    feature->set_tensor_length(3);

    // This method is invoked from SegmentationModelHandler whenever a model has
    // been updated and every time at startup. This will first read the old info
    // from the database, and then write the merged result of the old and new to
    // the database.
    ASSERT_TRUE(model_provider_data_.model_providers_callbacks.count(
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
    model_provider_data_
        .model_providers_callbacks
            [SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE]
        .Run(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, metadata,
             kModelVersion);
    segment_db_->UpdateCallback(true);

    // The SignalFilterProcessor needs to read the segment information from the
    // database before starting to listen to the updated signals.
    task_environment_.RunUntilIdle();
    //  We should have started recording 1 value histogram, once.
    EXPECT_EQ(
        1,
        histogram_tester.GetBucketCount(
            "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 1));

    AssertSelectedSegment(kTestSegmentationKey1, true,
                          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    AssertSelectedSegment(kTestSegmentationKey2, false);
    AssertSelectedSegment(kTestSegmentationKey3, false);
    AssertCachedSegment(kTestSegmentationKey1, true,
                        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    AssertCachedSegment(kTestSegmentationKey2, false);
    AssertCachedSegment(kTestSegmentationKey3, false);

    task_environment_.RunUntilIdle();

    ASSERT_TRUE(model_provider_data_.model_providers_callbacks.count(
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
    model_provider_data_
        .model_providers_callbacks
            [SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE]
        .Run(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE, metadata,
             kModelVersion);
    segment_db_->UpdateCallback(true);

    // The SignalFilterProcessor needs to read the segment information from the
    // database before starting to listen to the updated signals.
    task_environment_.RunUntilIdle();
    //  We should have started recording 1 value histogram, twice.
    EXPECT_EQ(
        2,
        histogram_tester.GetBucketCount(
            "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 1));

    // Database maintenance tasks should try to cleanup the signals after a
    // short delay, which starts with looking up data from the
    // SegmentInfoDatabase.
    task_environment_.FastForwardBy(base::Hours(1));

    AssertSelectedSegment(kTestSegmentationKey1, true,
                          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    AssertSelectedSegment(kTestSegmentationKey2, false);
    AssertSelectedSegment(kTestSegmentationKey3, false);
    AssertCachedSegment(kTestSegmentationKey1, true,
                        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    AssertCachedSegment(kTestSegmentationKey2, false);
    AssertCachedSegment(kTestSegmentationKey3, false);

    ASSERT_TRUE(
        model_provider_data_.default_provider_metadata.count(
            SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER) == 1);
  }

  void FailInitializationFlow() {
    // Let the DB loading fail.
    EXPECT_CALL(observer_, OnServiceStatusChanged(true, 7));
    segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
    signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    segment_storage_config_db_->InitStatusCallback(
        leveldb_proto::Enums::InitStatus::kOK);
    segment_storage_config_db_->LoadCallback(true);
  }

  int GetPendingActionsQueueSize() {
    return segmentation_platform_service_impl_->pending_actions_.size();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockServiceProxyObserver observer_;
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  std::unique_ptr<UkmDataManager> ukm_data_manager_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(SegmentationPlatformServiceImplTest, InitializationFlow) {
  TestInitializationFlow();
}

TEST_F(SegmentationPlatformServiceImplTest,
       GetSelectedSegmentBeforeInitialization) {
  SegmentSelectionResult expected;
  expected.is_ready = true;
  expected.segment = proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  base::RunLoop loop;
  segmentation_platform_service_impl_->GetSelectedSegment(
      kTestSegmentationKey1,
      base::BindOnce(&SegmentationPlatformServiceImplTest::OnGetSelectedSegment,
                     base::Unretained(this), loop.QuitClosure(), expected));
  loop.Run();
}

class SegmentationPlatformServiceImplEmptyConfigTest
    : public SegmentationPlatformServiceImplTest {
  std::vector<std::unique_ptr<Config>> CreateConfigs() override {
    return std::vector<std::unique_ptr<Config>>();
  }
};

TEST_F(SegmentationPlatformServiceImplEmptyConfigTest, InitializationFlow) {
  // Let the DB loading complete successfully.
  EXPECT_CALL(observer_, OnServiceStatusChanged(true, 7));
  segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  segment_storage_config_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  segment_storage_config_db_->LoadCallback(true);
  signal_db_->LoadCallback(true);

  // If initialization is succeeded, model execution scheduler should start
  // querying segment db.
  segment_db_->LoadCallback(true);
}

class SegmentationPlatformServiceImplMultiClientTest
    : public SegmentationPlatformServiceImplTest {
  void SetUpPrefs() override {
    ScopedDictPrefUpdate update(&pref_service_, kSegmentationResultPref);
    base::Value::Dict& dictionary = update.Get();

    base::Value::Dict segmentation_result;
    segmentation_result.Set("segment_id",
                            SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    dictionary.Set(kTestSegmentationKey1, std::move(segmentation_result));

    base::Value::Dict segmentation_result2;
    segmentation_result2.Set("segment_id",
                             SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
    dictionary.Set(kTestSegmentationKey2, std::move(segmentation_result2));
  }
};

TEST_F(SegmentationPlatformServiceImplMultiClientTest, InitializationFlow) {
  // Let the DB loading complete successfully.
  EXPECT_CALL(observer_, OnServiceStatusChanged(true, 7));
  segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  signal_db_->LoadCallback(true);
  segment_storage_config_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  segment_storage_config_db_->LoadCallback(true);

  // If initialization is succeeded, model execution scheduler should start
  // querying segment db.
  segment_db_->LoadCallback(true);

  AssertSelectedSegment(kTestSegmentationKey1, true,
                        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  AssertSelectedSegment(kTestSegmentationKey2, true,
                        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  AssertSelectedSegment(kTestSegmentationKey3, false);
  AssertCachedSegment(kTestSegmentationKey1, true,
                      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  AssertCachedSegment(kTestSegmentationKey2, true,
                      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  AssertCachedSegment(kTestSegmentationKey3, false);
}

class SegmentationPlatformDummyUkmManagerTest
    : public SegmentationPlatformServiceImplTest {
 public:
  SegmentationPlatformDummyUkmManagerTest()
      : SegmentationPlatformServiceImplTest(
            std::make_unique<DummyUkmDataManager>()) {}
};

TEST_F(SegmentationPlatformDummyUkmManagerTest, InitializationFlow) {
  TestInitializationFlow();
}

}  // namespace segmentation_platform
