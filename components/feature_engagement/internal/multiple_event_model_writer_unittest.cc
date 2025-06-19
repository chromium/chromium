// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/multiple_event_model_writer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feature_engagement/internal/event_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Sequence;

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

class MultipleEventModelWriterTest : public testing::Test {
 public:
  MultipleEventModelWriterTest() = default;
  MultipleEventModelWriterTest(const MultipleEventModelWriterTest&) = delete;
  MultipleEventModelWriterTest& operator=(const MultipleEventModelWriterTest&) =
      delete;
  ~MultipleEventModelWriterTest() override = default;

  void SetUp() override {
    profile_mocked_model_ = std::make_unique<MockEventModel>();
    device_mocked_model_ = std::make_unique<MockEventModel>();
    model_ = std::make_unique<MultipleEventModelWriter>(
        profile_mocked_model_.get(), device_mocked_model_.get());
  }

 protected:
  std::unique_ptr<MockEventModel> profile_mocked_model_;
  std::unique_ptr<MockEventModel> device_mocked_model_;
  std::unique_ptr<MultipleEventModelWriter> model_;
};

TEST_F(MultipleEventModelWriterTest, IncrementEventForAllEventModels) {
  Sequence sequence;
  EXPECT_CALL(*profile_mocked_model_.get(), IncrementEvent("bar", 1U))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*device_mocked_model_.get(), IncrementEvent("bar", 1U))
      .Times(1)
      .InSequence(sequence);

  model_->IncrementEvent("bar", 1U);
}

TEST_F(MultipleEventModelWriterTest, ClearEventForAllEventModels) {
  Sequence sequence;
  EXPECT_CALL(*profile_mocked_model_.get(), ClearEvent("bar"))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*device_mocked_model_.get(), ClearEvent("bar"))
      .Times(1)
      .InSequence(sequence);

  model_->ClearEvent("bar");
}

TEST_F(MultipleEventModelWriterTest, IncrementSnoozeForAllEventModels) {
  Sequence sequence;
  EXPECT_CALL(*profile_mocked_model_.get(), IncrementSnooze("bar", _, _))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*device_mocked_model_.get(), IncrementSnooze("bar", _, _))
      .Times(1)
      .InSequence(sequence);

  model_->IncrementSnooze("bar", 1U, base::Time::Now());
}

TEST_F(MultipleEventModelWriterTest, DismissSnoozeForAllEventModels) {
  Sequence sequence;
  EXPECT_CALL(*profile_mocked_model_.get(), DismissSnooze("bar"))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*device_mocked_model_.get(), DismissSnooze("bar"))
      .Times(1)
      .InSequence(sequence);

  model_->DismissSnooze("bar");
}

}  // namespace feature_engagement
