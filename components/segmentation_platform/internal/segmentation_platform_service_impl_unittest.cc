// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database_impl.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB

namespace segmentation_platform {
namespace {

std::string kTestSegmentationKey = "some_key";

Config CreateTestConfig() {
  Config config;
  config.segmentation_key = kTestSegmentationKey;
  config.segment_selection_ttl = base::TimeDelta::FromDays(28);
  config.segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
  return config;
}
}  // namespace

class SegmentationPlatformServiceImplTest : public testing::Test {
 public:
  SegmentationPlatformServiceImplTest() = default;
  ~SegmentationPlatformServiceImplTest() override = default;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    auto segment_db =
        std::make_unique<leveldb_proto::test::FakeDB<proto::SegmentInfo>>(
            &segment_db_entries_);
    auto signal_db =
        std::make_unique<leveldb_proto::test::FakeDB<proto::SignalData>>(
            &signal_db_entries_);
    auto segment_storage_config_db = std::make_unique<
        leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>>(
        &segment_storage_config_db_entries_);
    segment_db_ = segment_db.get();
    signal_db_ = signal_db.get();
    segment_storage_config_db_ = segment_storage_config_db.get();

    SegmentationPlatformService::RegisterProfilePrefs(pref_service_.registry());
    SetUpPrefs(OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

    auto config = std::make_unique<Config>(CreateTestConfig());
    config_ = config.get();
    segmentation_platform_service_impl_ =
        std::make_unique<SegmentationPlatformServiceImpl>(
            std::move(segment_db), std::move(signal_db),
            std::move(segment_storage_config_db), &model_provider_,
            &pref_service_, task_runner_, &test_clock_, std::move(config));
  }

  void TearDown() override {
    segmentation_platform_service_impl_.reset();
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    task_runner_->RunUntilIdle();
  }

  void SetUpPrefs(OptimizationTarget segment_id) {
    DictionaryPrefUpdate update(&pref_service_, kSegmentationResultPref);
    base::DictionaryValue* dictionary = update.Get();

    base::Value segmentation_result(base::Value::Type::DICTIONARY);
    segmentation_result.SetIntKey("segment_id", segment_id);
    dictionary->SetKey(kTestSegmentationKey, std::move(segmentation_result));
  }

  void OnGetSelectedSegment(base::RepeatingClosure closure,
                            const SegmentSelectionResult& expected,
                            const SegmentSelectionResult& actual) {
    ASSERT_EQ(expected, actual);
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  Config* config_;
  std::map<std::string, proto::SegmentInfo> segment_db_entries_;
  std::map<std::string, proto::SignalData> signal_db_entries_;
  std::map<std::string, proto::SignalStorageConfigs>
      segment_storage_config_db_entries_;
  leveldb_proto::test::FakeDB<proto::SegmentInfo>* segment_db_;
  leveldb_proto::test::FakeDB<proto::SignalData>* signal_db_;
  leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>*
      segment_storage_config_db_;
  optimization_guide::TestOptimizationGuideModelProvider model_provider_;
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<SegmentationPlatformServiceImpl>
      segmentation_platform_service_impl_;
};

TEST_F(SegmentationPlatformServiceImplTest, InitializationFlow) {
  // Let the DB loading complete successfully.
  segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  segment_storage_config_db_->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  segment_storage_config_db_->LoadCallback(true);

  // If initialization is succeeded, model execution scheduler should start
  // querying segment db.
  segment_db_->LoadCallback(true);

  // If we build with TF Lite, we need to also inspect whether the
  // ModelExecutionManagerImpl is publishing the correct data and whether that
  // leads to the SegmentationPlatformServiceImpl doing the right thing.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
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

  ModelExecutionManagerImpl* mem_impl = static_cast<ModelExecutionManagerImpl*>(
      segmentation_platform_service_impl_->model_execution_manager_.get());

  // This method is invoked from SegmentationModelHandler whenever a model has
  // been updated and every time at startup. This will first read the old info
  // from the database, and then write the merged result of the old and new to
  // the database.
  base::HistogramTester histogram_tester;
  mem_impl->OnSegmentationModelUpdated(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, metadata);
  segment_db_->GetCallback(true);
  segment_db_->UpdateCallback(true);

  // Since the updated config had a new feature, the SignalStorageConfigs DB
  // should have been updated.
  segment_storage_config_db_->UpdateCallback(true);

  // The SignalFilterProcessor needs to read the segment information from the
  // database before starting to listen to the updated signals.
  segment_db_->LoadCallback(true);
  //  We should have started recording 1 value histogram, once.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 1));
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  // Database maintenance tasks should try to cleanup the signals after a short
  // delay, which starts with looking up data from the SegmentInfoDatabase.
  task_environment_.FastForwardUntilNoTasksRemain();
  segment_db_->LoadCallback(true);
}

TEST_F(SegmentationPlatformServiceImplTest,
       GetSelectedSegmentBeforeInitialization) {
  SegmentSelectionResult expected;
  expected.is_ready = true;
  expected.segment = OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  base::RunLoop loop;
  segmentation_platform_service_impl_->GetSelectedSegment(
      kTestSegmentationKey,
      base::BindOnce(&SegmentationPlatformServiceImplTest::OnGetSelectedSegment,
                     base::Unretained(this), loop.QuitClosure(), expected));
  loop.Run();
}

}  // namespace segmentation_platform
