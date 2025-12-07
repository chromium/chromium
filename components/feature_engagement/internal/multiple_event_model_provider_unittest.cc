// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/multiple_event_model_provider.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace feature_engagement {

class MockEventModel : public EventModel {
 public:
  MockEventModel() = default;

  MockEventModel(const MockEventModel&) = delete;
  MockEventModel& operator=(const MockEventModel&) = delete;

  ~MockEventModel() override = default;

  // EventModel implementation.
  MOCK_METHOD2(Initialize, void(OnModelInitializationFinished, uint32_t));
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_CONST_METHOD1(GetEvent, Event*(const std::string&));
  MOCK_CONST_METHOD3(GetEventCount,
                     uint32_t(const std::string&, uint32_t, uint32_t));
  MOCK_METHOD2(IncrementEvent, void(const std::string&, uint32_t));
  MOCK_METHOD1(ClearEvent, void(const std::string&));
  MOCK_METHOD3(IncrementSnooze, void(const std::string&, uint32_t, base::Time));
  MOCK_METHOD1(DismissSnooze, void(const std::string&));
  MOCK_CONST_METHOD1(GetLastSnoozeTimestamp, base::Time(const std::string&));
  MOCK_CONST_METHOD3(GetSnoozeCount,
                     uint32_t(const std::string&, uint32_t, uint32_t));
  MOCK_CONST_METHOD1(IsSnoozeDismissed, bool(const std::string&));
};

class MultipleEventModelProviderTest : public testing::Test {
 public:
  MultipleEventModelProviderTest() {
    load_callback_ =
        base::BindOnce(&MultipleEventModelProviderTest::OnModelInitialized,
                       base::Unretained(this));
  }

  MultipleEventModelProviderTest(const MultipleEventModelProviderTest&) =
      delete;
  MultipleEventModelProviderTest& operator=(
      const MultipleEventModelProviderTest&) = delete;
  ~MultipleEventModelProviderTest() override = default;

  void SetUp() override {
    auto profile_mocked_model = std::make_unique<MockEventModel>();
    profile_mocked_model_ = profile_mocked_model.get();

    auto device_mocked_model = std::make_unique<MockEventModel>();
    device_mocked_model_ = device_mocked_model.get();

    model_ = std::make_unique<MultipleEventModelProvider>(
        std::move(profile_mocked_model), std::move(device_mocked_model));
  }

 protected:
  void OnModelInitialized(bool success) { load_success_ = success; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MultipleEventModelProvider> model_;
  raw_ptr<MockEventModel> profile_mocked_model_;
  raw_ptr<MockEventModel> device_mocked_model_;

  // Load callback tracking.
  std::optional<bool> load_success_;
  EventModel::OnModelInitializationFinished load_callback_;
};

TEST_F(MultipleEventModelProviderTest,
       SuccessfulInitializationForAllEventModels) {
  EventModel::OnModelInitializationFinished callback;
  EXPECT_CALL(*profile_mocked_model_, Initialize(_, _))
      .WillOnce([](EventModel::OnModelInitializationFinished cb,
                   uint32_t /*day*/) { std::move(cb).Run(true); });
  EXPECT_CALL(*device_mocked_model_, Initialize(_, _))
      .WillOnce([](EventModel::OnModelInitializationFinished cb,
                   uint32_t /*day*/) { std::move(cb).Run(true); });

  model_->Initialize(std::move(load_callback_), 2U);
  EXPECT_TRUE(load_success_.value());
}

TEST_F(MultipleEventModelProviderTest, FailInitializationWhenOneModelFails) {
  EventModel::OnModelInitializationFinished callback;
  EXPECT_CALL(*profile_mocked_model_, Initialize(_, _))
      .WillOnce([](EventModel::OnModelInitializationFinished callback,
                   uint32_t /*day*/) { std::move(callback).Run(true); });
  EXPECT_CALL(*device_mocked_model_, Initialize(_, _))
      .WillOnce([](EventModel::OnModelInitializationFinished callback,
                   uint32_t /*day*/) { std::move(callback).Run(false); });

  model_->Initialize(std::move(load_callback_), 2U);
  EXPECT_FALSE(load_success_.value());
}

TEST_F(MultipleEventModelProviderTest, IsReadyWhenAllEventModelsAreReady) {
  EXPECT_CALL(*profile_mocked_model_, IsReady())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*device_mocked_model_, IsReady()).Times(1).WillOnce(Return(true));
  EXPECT_TRUE(model_->IsReady());
}

TEST_F(MultipleEventModelProviderTest, IsNotReadyWhenOneModelIsNotReady) {
  EXPECT_CALL(*profile_mocked_model_, IsReady())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*device_mocked_model_, IsReady())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(model_->IsReady());
}

TEST_F(MultipleEventModelProviderTest,
       WhenProfileStorageIsRequestedGetEventCallsProfileModel) {
  FeatureConfig config;
  config.storage_type = StorageType::PROFILE;

  EXPECT_EQ(model_->GetEventModelReaderForFeature(config),
            profile_mocked_model_);
}

TEST_F(MultipleEventModelProviderTest,
       WhenDeviceStorageIsRequestedGetEventCallsDeviceModel) {
  FeatureConfig config;
  config.storage_type = StorageType::DEVICE;

  EXPECT_EQ(model_->GetEventModelReaderForFeature(config),
            device_mocked_model_);
}

}  // namespace feature_engagement
