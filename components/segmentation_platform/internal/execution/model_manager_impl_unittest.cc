// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_manager_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/model_manager.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;

namespace segmentation_platform {
namespace {

const int64_t kOldModelVersion = 100;
const int64_t kModelVersion = 123;

constexpr SegmentId kSearchUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;

constexpr SegmentId kPasswordManagerUserSegmentId =
    SegmentId::PASSWORD_MANAGER_USER;

using Sample = SignalDatabase::Sample;

class MockSegmentInfoDatabase : public test::TestSegmentInfoDatabase {
 public:
  MOCK_METHOD(void, Initialize, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              GetSegmentInfoForSegments,
              (const base::flat_set<SegmentId>& segment_ids,
               MultipleSegmentInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetSegmentInfo,
              (SegmentId segment_id,
               proto::ModelSource model_source,
               SegmentInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateSegment,
              (SegmentId segment_id,
               ModelSource model_source,
               std::optional<proto::SegmentInfo> segment_info,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              SaveSegmentResult,
              (SegmentId segment_id,
               ModelSource model_source,
               std::optional<proto::PredictionResult> result,
               SuccessCallback callback),
              (override));
};

}  // namespace

class ModelManagerTest : public testing::Test {
 public:
  ModelManagerTest() : model_provider_factory_(&model_provider_data_) {}
  ~ModelManagerTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    signal_database_ = std::make_unique<MockSignalDatabase>();
    clock_.SetNow(base::Time::Now());
    // Initialize DB and default models.
    model_provider_data_.segments_supporting_default_model = {
        kSearchUserSegmentId, kPasswordManagerUserSegmentId};
  }

  void TearDown() override {
    model_manager_.reset();
    // Allow for the SegmentationModelExecutor owned by ModelProvider
    // to be destroyed.
    RunUntilIdle();
  }

  void CreateModelManager(
      const base::flat_set<SegmentId>& segment_ids,
      const ModelManager::SegmentationModelUpdatedCallback& callback) {
    model_manager_ = std::make_unique<ModelManagerImpl>(
        segment_ids, &model_provider_factory_, &clock_, segment_database_.get(),
        callback);
    model_manager_->Initialize();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockModelProvider& FindHandler(proto::SegmentId segment_id) {
    return *(*model_provider_data_.model_providers.find(segment_id)).second;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  TestModelProviderFactory::Data model_provider_data_;
  TestModelProviderFactory model_provider_factory_;

  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  std::unique_ptr<ModelManagerImpl> model_manager_;
};

TEST_F(ModelManagerTest, OnSegmentationModelUpdatedInvalidMetadata) {
  // Use a MockSegmentInfoDatabase for this test in particular, to verify that
  // it is never used.
  auto mock_segment_database = std::make_unique<MockSegmentInfoDatabase>();
  auto* mock_segment_database_ptr = mock_segment_database.get();
  segment_database_ = std::move(mock_segment_database);

  // Construct the ModelManager.
  base::MockCallback<ModelManager::SegmentationModelUpdatedCallback> callback;
  auto segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelManager({segment_id}, callback.Get());

  // Create invalid metadata, which should be ignored.
  proto::SegmentInfo segment_info;
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);

  // Verify that the ModelManager never invokes its
  // SegmentInfoDatabase, nor invokes the callback.
  EXPECT_CALL(*mock_segment_database_ptr, GetSegmentInfo(_, _, _)).Times(0);
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  model_provider_data_.model_providers_callbacks[segment_id].Run(
      segment_id, metadata, kModelVersion);
}

TEST_F(ModelManagerTest, OnSegmentationModelUpdatedNoOldMetadata) {
  base::MockCallback<ModelManager::SegmentationModelUpdatedCallback> callback;
  auto segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelManager({segment_id}, callback.Get());

  proto::SegmentInfo segment_info;
  segment_info.set_model_source(proto::ModelSource::DEFAULT_MODEL_SOURCE);
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42u);
  metadata.set_time_unit(proto::TimeUnit::DAY);
  EXPECT_CALL(callback, Run(_, std::optional<int64_t>()))
      .WillOnce(SaveArg<0>(&segment_info));
  model_provider_data_.model_providers_callbacks[segment_id].Run(
      segment_id, metadata, kModelVersion);

  // Verify that the resulting callback was invoked correctly.
  EXPECT_EQ(segment_id, segment_info.segment_id());
  EXPECT_EQ(42u, segment_info.model_metadata().bucket_duration());
  EXPECT_EQ(proto::ModelSource::SERVER_MODEL_SOURCE,
            segment_info.model_source());

  // Also verify that the database has been updated.
  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback;
  std::optional<proto::SegmentInfo> segment_info_from_db;
  EXPECT_CALL(db_callback, Run(_)).WillOnce(SaveArg<0>(&segment_info_from_db));

  // Fetch SegmentInfo from the database.
  segment_database_->GetSegmentInfo(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, db_callback.Get());
  EXPECT_TRUE(segment_info_from_db.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db->segment_id());

  // The metadata should have been stored.
  EXPECT_EQ(42u, segment_info_from_db->model_metadata().bucket_duration());

  // Model update time should be updated.
  EXPECT_EQ(clock_.Now().ToDeltaSinceWindowsEpoch().InSeconds(),
            segment_info_from_db->model_update_time_s());
}

TEST_F(
    ModelManagerTest,
    OnSegmentationModelUpdatedWithPreviousMetadataAndPredictionResultAndTrainingData) {
  base::MockCallback<ModelManager::SegmentationModelUpdatedCallback> callback;
  auto segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelManager({segment_id}, callback.Get());

  // Fill in old data in the SegmentInfo database.
  segment_database_->SetBucketDuration(segment_id, 456, proto::TimeUnit::MONTH);
  segment_database_->AddUserActionFeature(segment_id, "hello", 2, 2,
                                          proto::Aggregation::BUCKETED_COUNT);
  segment_database_->AddPredictionResult(segment_id, 2, clock_.Now());

  proto::TrainingData training_data;
  training_data.add_inputs(1);
  training_data.set_request_id(
      TrainingRequestId::FromUnsafeValue(1).GetUnsafeValue());
  // Store a training data request to the DB.
  segment_database_->SaveTrainingData(segment_id,
                                      proto::ModelSource::SERVER_MODEL_SOURCE,
                                      training_data, base::DoNothing());

  segment_database_->FindOrCreateSegment(segment_id)
      ->set_model_version(kOldModelVersion);

  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_1;
  std::optional<proto::SegmentInfo> segment_info_from_db_1;
  EXPECT_CALL(db_callback_1, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_1));
  segment_database_->GetSegmentInfo(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, db_callback_1.Get());
  EXPECT_TRUE(segment_info_from_db_1.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_1->segment_id());

  // Verify the old metadata and prediction result and training data has been
  // stored correctly.
  EXPECT_EQ(456u, segment_info_from_db_1->model_metadata().bucket_duration());
  EXPECT_THAT(segment_info_from_db_1->prediction_result().result(),
              testing::ElementsAre(2));
  EXPECT_EQ(ModelSource::SERVER_MODEL_SOURCE,
            segment_info_from_db_1->model_source());
  EXPECT_EQ(1, segment_info_from_db_1->training_data_size());

  // Verify the metadata features have been stored correctly.
  EXPECT_EQ(proto::SignalType::USER_ACTION,
            segment_info_from_db_1->model_metadata()
                .input_features(0)
                .uma_feature()
                .type());
  EXPECT_EQ("hello", segment_info_from_db_1->model_metadata()
                         .input_features(0)
                         .uma_feature()
                         .name());
  EXPECT_EQ(proto::Aggregation::BUCKETED_COUNT,
            segment_info_from_db_1->model_metadata()
                .input_features(0)
                .uma_feature()
                .aggregation());

  // Create segment info that does not match.
  proto::SegmentInfo segment_info;
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42u);
  metadata.set_time_unit(proto::TimeUnit::HOUR);

  // Create one feature that does not match the stored feature.
  auto* feature = metadata.add_features();
  feature->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature->set_name("other");
  // Intentionally not set the name hash, as it should be set automatically.
  feature->set_aggregation(proto::Aggregation::BUCKETED_SUM);
  feature->set_bucket_count(3);
  feature->set_tensor_length(3);

  // Invoke the callback and store the resulting invocation of the outer
  // callback for verification.
  EXPECT_CALL(callback, Run(_, std::optional<int64_t>(kOldModelVersion)))
      .WillOnce(SaveArg<0>(&segment_info));
  model_provider_data_.model_providers_callbacks[segment_id].Run(
      segment_id, metadata, kModelVersion);

  // Should now have the metadata from the new proto.
  EXPECT_EQ(segment_id, segment_info.segment_id());
  EXPECT_EQ(42u, segment_info.model_metadata().bucket_duration());
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE,
            segment_info.model_metadata().features(0).type());
  EXPECT_EQ("other", segment_info.model_metadata().features(0).name());
  // The name_hash should have been set automatically.
  EXPECT_EQ(base::HashMetricName("other"),
            segment_info.model_metadata().features(0).name_hash());
  EXPECT_EQ(proto::Aggregation::BUCKETED_SUM,
            segment_info.model_metadata().features(0).aggregation());
  // The `prediction_result` should be empty.
  EXPECT_TRUE(segment_info.prediction_result().result().size() == 0);

  // Also verify that the database has been updated.
  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_2;
  std::optional<proto::SegmentInfo> segment_info_from_db_2;
  EXPECT_CALL(db_callback_2, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_2));
  segment_database_->GetSegmentInfo(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, db_callback_2.Get());
  EXPECT_TRUE(segment_info_from_db_2.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_2->segment_id());
  EXPECT_EQ(clock_.Now().ToDeltaSinceWindowsEpoch().InSeconds(),
            segment_info_from_db_2->model_update_time_s());

  // The metadata should have been updated.
  EXPECT_EQ(42u, segment_info_from_db_2->model_metadata().bucket_duration());
  // The metadata features should have been updated.
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE,
            segment_info_from_db_2->model_metadata().features(0).type());
  EXPECT_EQ("other",
            segment_info_from_db_2->model_metadata().features(0).name());
  EXPECT_EQ(base::HashMetricName("other"),
            segment_info_from_db_2->model_metadata().features(0).name_hash());
  EXPECT_EQ(proto::Aggregation::BUCKETED_SUM,
            segment_info_from_db_2->model_metadata().features(0).aggregation());
  // The `prediction_result` should be empty.
  EXPECT_TRUE(segment_info.prediction_result().result().size() == 0);
}

TEST_F(ModelManagerTest, DatabaseUpdateForDeletedServerModel) {
  auto segment_id = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;

  base::MockCallback<ModelManager::SegmentationModelUpdatedCallback>
      model_updated_callback;
  proto::SegmentInfo updated_segment_info;
  EXPECT_CALL(model_updated_callback,
              Run(_, std::optional<int64_t>(kOldModelVersion)))
      .WillOnce(SaveArg<0>(&updated_segment_info));

  // Fill in old data for a server model in the SegmentInfo database.
  segment_database_->SetBucketDuration(segment_id, 456, proto::TimeUnit::MONTH,
                                       proto::ModelSource::SERVER_MODEL_SOURCE);
  segment_database_->AddUserActionFeature(
      segment_id, /*user_action=*/"hello", /*bucket_count=*/2,
      /*tensor_length=*/2, proto::Aggregation::BUCKETED_COUNT,
      proto::ModelSource::SERVER_MODEL_SOURCE);
  segment_database_->AddPredictionResult(
      segment_id, 2, clock_.Now(), proto::ModelSource::SERVER_MODEL_SOURCE);
  segment_database_->FindOrCreateSegment(segment_id)
      ->set_model_version(kOldModelVersion);

  CreateModelManager({segment_id}, model_updated_callback.Get());

  // If the server stops serving a model then we'll receive a callback with null
  // metadata.
  model_provider_data_.model_providers_callbacks[segment_id].Run(
      segment_id, /* metadata = */ std::nullopt,
      /* model_version = */ kModelVersion);

  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback;
  std::optional<proto::SegmentInfo> segment_info_from_db;
  EXPECT_CALL(db_callback, Run(_)).WillOnce(SaveArg<0>(&segment_info_from_db));

  // Try to get data from segment DB, it should have been deleted.
  segment_database_->GetSegmentInfo(segment_id, proto::SERVER_MODEL_SOURCE,
                                    db_callback.Get());
  EXPECT_FALSE(segment_info_from_db.has_value());

  // ModelManager should have called its SegmentationModelUpdatedCallback with a
  // SegmentInfo without metadata.
  EXPECT_EQ(updated_segment_info.segment_id(), segment_id);
  EXPECT_EQ(updated_segment_info.model_source(), proto::SERVER_MODEL_SOURCE);
  EXPECT_FALSE(updated_segment_info.has_model_metadata());
}

TEST_F(ModelManagerTest, DatabaseUpdateForDefaultModel) {
  auto segment_id = kSearchUserSegmentId;
  // Fill in old data for default model in the SegmentInfo database.
  segment_database_->SetBucketDuration(
      segment_id, 456, proto::TimeUnit::MONTH,
      proto::ModelSource::DEFAULT_MODEL_SOURCE);
  segment_database_->AddUserActionFeature(
      segment_id, /*user_action=*/"hello", /*bucket_count=*/2,
      /*tensor_length=*/2, proto::Aggregation::BUCKETED_COUNT,
      proto::ModelSource::DEFAULT_MODEL_SOURCE);
  segment_database_->AddPredictionResult(
      segment_id, 2, clock_.Now(), proto::ModelSource::DEFAULT_MODEL_SOURCE);

  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_1;
  std::optional<proto::SegmentInfo> segment_info_from_db_1;
  EXPECT_CALL(db_callback_1, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_1));
  segment_database_->GetSegmentInfo(
      segment_id, ModelSource::DEFAULT_MODEL_SOURCE, db_callback_1.Get());
  EXPECT_TRUE(segment_info_from_db_1.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_1->segment_id());

  // Verify the old metadata and prediction result has been stored correctly for
  // default model.
  EXPECT_EQ(456u, segment_info_from_db_1->model_metadata().bucket_duration());
  EXPECT_THAT(segment_info_from_db_1->prediction_result().result(),
              testing::ElementsAre(2));
  EXPECT_EQ(ModelSource::DEFAULT_MODEL_SOURCE,
            segment_info_from_db_1->model_source());

  // Verify the metadata features have been stored correctly for default model.
  EXPECT_EQ(proto::SignalType::USER_ACTION,
            segment_info_from_db_1->model_metadata()
                .input_features(0)
                .uma_feature()
                .type());
  EXPECT_EQ("hello", segment_info_from_db_1->model_metadata()
                         .input_features(0)
                         .uma_feature()
                         .name());
  EXPECT_EQ(proto::Aggregation::BUCKETED_COUNT,
            segment_info_from_db_1->model_metadata()
                .input_features(0)
                .uma_feature()
                .aggregation());

  CreateModelManager({segment_id}, base::DoNothing());

  // Also verify that the database has been updated.
  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_2;
  std::optional<proto::SegmentInfo> segment_info_from_db_2;
  EXPECT_CALL(db_callback_2, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_2));
  segment_database_->GetSegmentInfo(segment_id,
                                    proto::ModelSource::DEFAULT_MODEL_SOURCE,
                                    db_callback_2.Get());
  EXPECT_TRUE(segment_info_from_db_2.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_2->segment_id());
  EXPECT_EQ(clock_.Now().ToDeltaSinceWindowsEpoch().InSeconds(),
            segment_info_from_db_2->model_update_time_s());

  // The metadata should have been updated.
  EXPECT_EQ(proto::TimeUnit::DAY,
            segment_info_from_db_2->model_metadata().time_unit());
}

TEST_F(ModelManagerTest, GetModelProvider) {
  CreateModelManager({kSearchUserSegmentId, kPasswordManagerUserSegmentId},
                     base::DoNothing());
  ASSERT_TRUE(model_manager_->GetModelProvider(
      kSearchUserSegmentId, proto::ModelSource::DEFAULT_MODEL_SOURCE));
  ASSERT_TRUE(model_manager_->GetModelProvider(
      kPasswordManagerUserSegmentId, proto::ModelSource::DEFAULT_MODEL_SOURCE));
  ASSERT_TRUE(model_manager_->GetModelProvider(
      kSearchUserSegmentId, proto::ModelSource::SERVER_MODEL_SOURCE));
  ASSERT_TRUE(model_manager_->GetModelProvider(
      kPasswordManagerUserSegmentId, proto::ModelSource::SERVER_MODEL_SOURCE));
}

}  // namespace segmentation_platform
