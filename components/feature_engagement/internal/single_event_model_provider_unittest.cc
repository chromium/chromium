// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_event_model_provider.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnArg;
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

class SingleEventModelProviderTest : public testing::Test {
 public:
  SingleEventModelProviderTest() {
    load_callback_ =
        base::BindOnce(&SingleEventModelProviderTest::OnModelInitialized,
                       base::Unretained(this));
  }

  SingleEventModelProviderTest(const SingleEventModelProviderTest&) = delete;
  SingleEventModelProviderTest& operator=(const SingleEventModelProviderTest&) =
      delete;
  ~SingleEventModelProviderTest() override = default;

  void SetUp() override {
    auto mocked_model = std::make_unique<MockEventModel>();
    mocked_model_ = mocked_model.get();
    provider_ =
        std::make_unique<SingleEventModelProvider>(std::move(mocked_model));
  }

 protected:
  void OnModelInitialized(bool success) { load_success_ = success; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SingleEventModelProvider> provider_;
  raw_ptr<MockEventModel> mocked_model_;

  // Load callback tracking.
  std::optional<bool> load_success_;
  EventModel::OnModelInitializationFinished load_callback_;
};

TEST_F(SingleEventModelProviderTest, SuccessfulInitializationForEventProvider) {
  EventModel::OnModelInitializationFinished callback;
  EXPECT_CALL(*mocked_model_, Initialize(_, _))
      .WillOnce([](EventModel::OnModelInitializationFinished cb,
                   uint32_t /*day*/) { std::move(cb).Run(true); });
  provider_->Initialize(std::move(load_callback_), 2U);
  EXPECT_TRUE(load_success_.value());
}

TEST_F(SingleEventModelProviderTest,
       EventProviderIsReadyWhenEventModelIsReady) {
  EXPECT_CALL(*mocked_model_, IsReady()).Times(1).WillOnce(Return(true));
  EXPECT_TRUE(provider_->IsReady());
}

TEST_F(SingleEventModelProviderTest, EventProviderReturnsReaderForGetEvent) {
  Event foo;
  foo.set_name("foo");
  test::SetEventCountForDay(&foo, 1, 1);
  FeatureConfig config;

  EXPECT_CALL(*mocked_model_, GetEvent(foo.name()))
      .Times(1)
      .WillRepeatedly(Return(&foo));

  test::VerifyEventsEqual(
      &foo,
      provider_->GetEventModelReaderForFeature(config)->GetEvent(foo.name()));
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsReaderForGetEventCount) {
  FeatureConfig config;

  EXPECT_CALL(*mocked_model_, GetEventCount("foo", _, _))
      .WillRepeatedly(ReturnArg<1>());

  EXPECT_EQ(2u, provider_->GetEventModelReaderForFeature(config)->GetEventCount(
                    "foo", 2, 1));
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsReaderForGetLastSnoozeTimestamp) {
  base::Time time = base::Time::Now();
  FeatureConfig config;

  EXPECT_CALL(*mocked_model_, GetLastSnoozeTimestamp("bar"))
      .Times(1)
      .WillOnce(Return(time));

  EXPECT_EQ(time, provider_->GetEventModelReaderForFeature(config)
                      ->GetLastSnoozeTimestamp("bar"));
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsReaderForGetSnoozeCount) {
  FeatureConfig config;

  EXPECT_CALL(*mocked_model_, GetSnoozeCount("foo", _, _))
      .WillRepeatedly(ReturnArg<1>());

  EXPECT_EQ(2u,
            provider_->GetEventModelReaderForFeature(config)->GetSnoozeCount(
                "foo", 2, 1));
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsReaderForIsSnoozeDismissed) {
  FeatureConfig config;

  EXPECT_CALL(*mocked_model_, IsSnoozeDismissed("foo"))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_TRUE(
      provider_->GetEventModelReaderForFeature(config)->IsSnoozeDismissed(
          "foo"));
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsWriterForIncrementEvent) {
  Sequence sequence;
  EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U))
      .Times(1)
      .InSequence(sequence);

  provider_->GetEventModelWriter()->IncrementEvent("bar", 1U);
}

TEST_F(SingleEventModelProviderTest, EventProviderReturnsWriterForClearEvent) {
  Sequence sequence;
  EXPECT_CALL(*mocked_model_, ClearEvent("bar")).Times(1).InSequence(sequence);

  provider_->GetEventModelWriter()->ClearEvent("bar");
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsWriterForIncrementSnooze) {
  Sequence sequence;
  EXPECT_CALL(*mocked_model_, IncrementSnooze("bar", _, _))
      .Times(1)
      .InSequence(sequence);

  provider_->GetEventModelWriter()->IncrementSnooze("bar", 1U,
                                                    base::Time::Now());
}

TEST_F(SingleEventModelProviderTest,
       EventProviderReturnsWriterForDismissSnooze) {
  Sequence sequence;
  EXPECT_CALL(*mocked_model_, DismissSnooze("bar"))
      .Times(1)
      .InSequence(sequence);

  provider_->GetEventModelWriter()->DismissSnooze("bar");
}

}  // namespace feature_engagement
