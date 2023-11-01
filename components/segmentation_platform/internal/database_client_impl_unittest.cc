// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database_client_impl.h"

#include "base/containers/flat_map.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include "components/segmentation_platform/internal/execution/model_manager.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

class MockModelManager : public ModelManager {
 public:
  MOCK_METHOD(ModelProvider*,
              GetModelProvider,
              (proto::SegmentId segment_id, proto::ModelSource model_source));

  MOCK_METHOD(void, Initialize, ());

  MOCK_METHOD(
      void,
      SetSegmentationModelUpdatedCallbackForTesting,
      (ModelManager::SegmentationModelUpdatedCallback model_updated_callback));
};

class DatabaseClientImplTest : public testing::Test {
 public:
  DatabaseClientImplTest() = default;
  ~DatabaseClientImplTest() override = default;

  void SetUp() override {
    Test::SetUp();
    clock_.Advance(base::Days(100));

    auto query_processor =
        std::make_unique<processing::MockFeatureListQueryProcessor>();
    query_processor_ = query_processor.get();
    execution_service_ = std::make_unique<ExecutionService>();
    model_manager_ = std::make_unique<MockModelManager>();
    execution_service_->InitForTesting(
        std::move(query_processor),
        std::make_unique<ModelExecutorImpl>(&clock_, nullptr, query_processor_),
        nullptr, model_manager_.get());
    data_manager_ = std::make_unique<MockUkmDataManager>();
    database_client_ = std::make_unique<DatabaseClientImpl>(
        execution_service_.get(), data_manager_.get());
  }

  void TearDown() override {
    database_client_.reset();
    data_manager_.reset();
    query_processor_ = nullptr;
    execution_service_.reset();
    model_manager_.reset();
    data_manager_.reset();

    Test::TearDown();
  }

 protected:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<processing::MockFeatureListQueryProcessor> query_processor_;
  std::unique_ptr<MockModelManager> model_manager_;
  std::unique_ptr<ExecutionService> execution_service_;
  std::unique_ptr<MockUkmDataManager> data_manager_;
  std::unique_ptr<DatabaseClientImpl> database_client_;
};

TEST_F(DatabaseClientImplTest, ProcessFeatures) {
  base::RunLoop wait1;
  EXPECT_CALL(
      *query_processor_,
      ProcessFeatureList(
          _, _, SegmentId::DATABASE_API_CLIENTS, clock_.Now(), base::Time(),
          processing::FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false, ModelProvider::Request{0},
                                   ModelProvider::Response()));
  database_client_->ProcessFeatures(
      proto::SegmentationModelMetadata(), clock_.Now(),
      base::BindOnce(
          [](base::OnceClosure quit, DatabaseClient::ResultStatus status,
             const ModelProvider::Request& result) {
            EXPECT_EQ(status, DatabaseClient::ResultStatus::kSuccess);
            EXPECT_EQ(result, ModelProvider::Request{0});
            std::move(quit).Run();
          },
          wait1.QuitClosure()));
  wait1.Run();

  base::RunLoop wait2;
  EXPECT_CALL(
      *query_processor_,
      ProcessFeatureList(
          _, _, SegmentId::DATABASE_API_CLIENTS, clock_.Now(), base::Time(),
          processing::FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true, ModelProvider::Request{0},
                                   ModelProvider::Response()));
  database_client_->ProcessFeatures(
      proto::SegmentationModelMetadata(), clock_.Now(),
      base::BindOnce(
          [](base::OnceClosure quit, DatabaseClient::ResultStatus status,
             const ModelProvider::Request& result) {
            EXPECT_EQ(status, DatabaseClient::ResultStatus::kError);
            EXPECT_EQ(result, ModelProvider::Request{0});
            std::move(quit).Run();
          },
          wait2.QuitClosure()));
  wait2.Run();
}

TEST_F(DatabaseClientImplTest, AddEntry) {
  MockUkmDatabase database;
  EXPECT_CALL(*data_manager_, GetUkmDatabase())
      .WillRepeatedly(Return(&database));

  EXPECT_CALL(database, StoreUkmEntry(_))
      .WillOnce([](ukm::mojom::UkmEntryPtr entry) {
        EXPECT_EQ(entry->event_hash, 11u);
      });
  DatabaseClient::StructuredEvent e;
  e.event_id = UkmEventHash::FromUnsafeValue(11);
  database_client_->AddEvent(e);

  EXPECT_CALL(database, StoreUkmEntry(_))
      .WillOnce([](ukm::mojom::UkmEntryPtr entry) {
        EXPECT_EQ(entry->event_hash, 12u);
        base::flat_map<uint64_t, int64_t> expected{{3, 5}, {4, 10}};
        EXPECT_EQ(entry->metrics, expected);
      });
  e.event_id = UkmEventHash::FromUnsafeValue(12);
  e.metric_hash_to_value[UkmMetricHash::FromUnsafeValue(3)] = 5;
  e.metric_hash_to_value[UkmMetricHash::FromUnsafeValue(4)] = 10;
  database_client_->AddEvent(e);
}

}  // namespace
}  // namespace segmentation_platform
