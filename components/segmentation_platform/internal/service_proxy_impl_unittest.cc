// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace segmentation_platform {

namespace {

constexpr char kTestSegmentationKey[] = "test_key";

// Adds a segment info into a map, and return a copy of it.
proto::SegmentInfo* AddSegmentInfo(
    test::TestSegmentInfoDatabase* segment_db,
    Config* config,
    SegmentId segment_id,
    ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE) {
  auto* info = segment_db->FindOrCreateSegment(segment_id, model_source);
  info->mutable_model_metadata()->set_time_unit(proto::TimeUnit::DAY);
  config->segments.insert(
      {segment_id, std::make_unique<Config::SegmentMetadata>("UmaName")});
  return info;
}

class MockModelExecutionScheduler : public ModelExecutionScheduler {
 public:
  MockModelExecutionScheduler() = default;
  MOCK_METHOD(void, RequestModelExecution, (const proto::SegmentInfo&));
  MOCK_METHOD(void, OnNewModelInfoReady, (const proto::SegmentInfo&));
  MOCK_METHOD(void, RequestModelExecutionForEligibleSegments, (bool));
  MOCK_METHOD(void,
              OnModelExecutionCompleted,
              (const proto::SegmentInfo&,
               (std::unique_ptr<ModelExecutionResult>)));
};

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

class MockModelExecutor : public ModelExecutor {
 public:
  MockModelExecutor() = default;
  MOCK_METHOD(void, ExecuteModel, (std::unique_ptr<ExecutionRequest>));
};

}  // namespace

class FakeSegmentSelectorImpl : public SegmentSelectorImpl {
 public:
  FakeSegmentSelectorImpl(TestingPrefServiceSimple* pref_service,
                          const Config* config)
      : SegmentSelectorImpl(nullptr,
                            nullptr,
                            pref_service,
                            config,
                            nullptr,
                            nullptr,
                            PlatformOptions::CreateDefault()) {}
  ~FakeSegmentSelectorImpl() override = default;

  void UpdateSelectedSegment(SegmentId new_selection, float) override {
    new_selection_ = new_selection;
  }

  SegmentId new_selection() const { return new_selection_; }

 private:
  SegmentId new_selection_;
};

class ServiceProxyImplTest : public testing::Test,
                             public ServiceProxy::Observer {
 public:
  ServiceProxyImplTest() = default;
  ~ServiceProxyImplTest() override = default;

  void SetUp() override {
    auto config = std::make_unique<Config>();
    config->segmentation_key = kTestSegmentationKey;
    configs_.emplace_back(std::move(config));
    pref_service_.registry()->RegisterDictionaryPref(kSegmentationResultPref);
    segment_db_ = std::make_unique<test::TestSegmentInfoDatabase>();
  }

  void SetUpProxy() {
    segment_selectors_[kTestSegmentationKey] =
        std::make_unique<FakeSegmentSelectorImpl>(&pref_service_,
                                                  configs_.at(0).get());
    service_proxy_impl_ = std::make_unique<ServiceProxyImpl>(
        segment_db_.get(), nullptr, &configs_, PlatformOptions(false),
        &segment_selectors_);
    service_proxy_impl_->AddObserver(this);
  }

  void TearDown() override {
    service_proxy_impl_.reset();
    segment_db_.reset();
    configs_.clear();
    client_info_.clear();
  }

  void OnServiceStatusChanged(bool is_initialized, int status_flag) override {
    is_initialized_ = is_initialized;
    status_flag_ = status_flag;
  }

  void OnClientInfoAvailable(
      const std::vector<ServiceProxy::ClientInfo>& client_info) override {
    client_info_ = client_info;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  bool is_initialized_ = false;
  int status_flag_ = 0;

  std::unique_ptr<test::TestSegmentInfoDatabase> segment_db_;
  std::unique_ptr<MockModelManager> mock_model_manager_;
  ExecutionService execution_;
  std::unique_ptr<ServiceProxyImpl> service_proxy_impl_;
  std::vector<ServiceProxy::ClientInfo> client_info_;
  std::vector<std::unique_ptr<Config>> configs_;
  base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>
      segment_selectors_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ServiceProxyImplTest, GetServiceStatus) {
  SetUpProxy();
  service_proxy_impl_->GetServiceStatus();
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 0);

  service_proxy_impl_->OnServiceStatusChanged(false, 1);
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 1);

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  ASSERT_EQ(is_initialized_, true);
  ASSERT_EQ(status_flag_, 7);

  ASSERT_FALSE(client_info_.empty());
  ASSERT_EQ(client_info_.size(), 1u);
}

TEST_F(ServiceProxyImplTest, GetSegmentationInfoFromDefaultModel) {
  const SegmentId segment_id =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  configs_.at(0)->segments.insert(
      {segment_id, std::make_unique<Config::SegmentMetadata>("UmaName")});
  AddSegmentInfo(segment_db_.get(), configs_.at(0).get(), segment_id,
                 proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);

  ASSERT_EQ(client_info_.size(), 1u);
  ASSERT_EQ(client_info_.at(0).segmentation_key, kTestSegmentationKey);
  ASSERT_EQ(client_info_.at(0).segment_status.size(), 1u);
  ServiceProxy::SegmentStatus status = client_info_.at(0).segment_status.at(0);
  ASSERT_EQ(status.segment_id,
            SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  ASSERT_EQ(status.can_execute_segment, false);
  ASSERT_EQ(status.segment_metadata, "model_metadata: { time_unit:DAY }");
  ASSERT_TRUE(status.prediction_result.empty());
}

TEST_F(ServiceProxyImplTest, GetSegmentationInfoFromDB) {
  const SegmentId segment_id =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  configs_.at(0)->segments.insert(
      {segment_id, std::make_unique<Config::SegmentMetadata>("UmaName")});

  AddSegmentInfo(segment_db_.get(), configs_.at(0).get(), segment_id,
                 proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);

  ASSERT_EQ(client_info_.size(), 1u);
  ASSERT_EQ(client_info_.at(0).segmentation_key, kTestSegmentationKey);
  ASSERT_EQ(client_info_.at(0).segment_status.size(), 1u);
  ServiceProxy::SegmentStatus status = client_info_.at(0).segment_status.at(0);
  ASSERT_EQ(status.segment_id,
            SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  ASSERT_EQ(status.can_execute_segment, false);
  ASSERT_EQ(status.segment_metadata, "model_metadata: { time_unit:DAY }");
  ASSERT_TRUE(status.prediction_result.empty());
}

TEST_F(ServiceProxyImplTest, ExecuteModel) {
  auto* info =
      AddSegmentInfo(segment_db_.get(), configs_.at(0).get(),
                     SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);

  segment_db_->UpdateSegment(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      ModelSource::SERVER_MODEL_SOURCE, *info, base::DoNothing());

  auto executor_moved = std::make_unique<MockModelExecutor>();
  MockModelExecutor* mock_executor = executor_moved.get();

  mock_model_manager_ = std::make_unique<MockModelManager>();
  execution_.InitForTesting(nullptr, std::move(executor_moved), nullptr,
                            mock_model_manager_.get());

  // ExecutionService is not set, ExecuteModel() will do nothing.
  EXPECT_CALL(*mock_executor, ExecuteModel(_)).Times(0);
  service_proxy_impl_->ExecuteModel(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);

  service_proxy_impl_->SetExecutionService(&execution_);
  EXPECT_CALL(*mock_executor, ExecuteModel(_)).Times(1);
  service_proxy_impl_->ExecuteModel(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);

  EXPECT_CALL(*mock_executor, ExecuteModel(_)).Times(0);
  service_proxy_impl_->ExecuteModel(SegmentId::OPTIMIZATION_TARGET_UNKNOWN);
}

TEST_F(ServiceProxyImplTest, OverwriteResult) {
  AddSegmentInfo(segment_db_.get(), configs_.at(0).get(),
                 SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);

  auto scheduler_moved = std::make_unique<MockModelExecutionScheduler>();
  MockModelExecutionScheduler* scheduler = scheduler_moved.get();
  execution_.InitForTesting(nullptr, nullptr, std::move(scheduler_moved),
                            nullptr);

  // Scheduler is not set, OverwriteValue() will do nothing.
  EXPECT_CALL(*scheduler, OnModelExecutionCompleted(_, _)).Times(0);
  service_proxy_impl_->OverwriteResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 0.7);

  service_proxy_impl_->SetExecutionService(&execution_);

  EXPECT_CALL(*scheduler, OnModelExecutionCompleted(_, _)).Times(1);
  service_proxy_impl_->OverwriteResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 0.7);

  EXPECT_CALL(*scheduler, OnModelExecutionCompleted(_, _)).Times(0);
  service_proxy_impl_->OverwriteResult(SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
                                       0.7);
}

TEST_F(ServiceProxyImplTest, SetSelectSegment) {
  AddSegmentInfo(segment_db_.get(), configs_.at(0).get(),
                 SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);

  service_proxy_impl_->SetSelectedSegment(
      kTestSegmentationKey,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  ASSERT_EQ(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
            static_cast<FakeSegmentSelectorImpl*>(
                segment_selectors_[kTestSegmentationKey].get())
                ->new_selection());
}
}  // namespace segmentation_platform
