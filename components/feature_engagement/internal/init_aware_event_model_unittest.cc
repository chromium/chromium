// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/init_aware_event_model.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnArg;
using testing::SaveArg;
using testing::Sequence;

namespace feature_engagement {

namespace {

class MockEventModel : public EventModel {
 public:
  MockEventModel() = default;
  ~MockEventModel() override = default;

  // EventModel implementation.
  MOCK_METHOD2(Initialize, void(OnModelInitializationFinished, uint32_t));
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_CONST_METHOD1(GetEvent, Event*(const std::string&));
  MOCK_CONST_METHOD3(GetEventCount,
                     uint32_t(const std::string&, uint32_t, uint32_t));
  MOCK_METHOD2(IncrementEvent, void(const std::string&, uint32_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventModel);
};

class InitAwareEventModelTest : public testing::Test {
 public:
  InitAwareEventModelTest() : mocked_model_(nullptr) {
    load_callback_ = base::BindOnce(
        &InitAwareEventModelTest::OnModelInitialized, base::Unretained(this));
  }

  ~InitAwareEventModelTest() override = default;

  void SetUp() override {
    auto mocked_model = std::make_unique<MockEventModel>();
    mocked_model_ = mocked_model.get();
    model_ = std::make_unique<InitAwareEventModel>(std::move(mocked_model));
  }

 protected:
  void OnModelInitialized(bool success) { load_success_ = success; }

  std::unique_ptr<InitAwareEventModel> model_;
  MockEventModel* mocked_model_;

  // Load callback tracking.
  base::Optional<bool> load_success_;
  EventModel::OnModelInitializationFinished load_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InitAwareEventModelTest);
};

}  // namespace

TEST_F(InitAwareEventModelTest, PassThroughIsReady) {
  EXPECT_CALL(*mocked_model_, IsReady()).Times(1);
  model_->IsReady();
}

TEST_F(InitAwareEventModelTest, PassThroughGetEvent) {
  Event foo;
  foo.set_name("foo");
  test::SetEventCountForDay(&foo, 1, 1);

  EXPECT_CALL(*mocked_model_, GetEvent(foo.name()))
      .WillRepeatedly(Return(&foo));
  EXPECT_CALL(*mocked_model_, GetEvent("bar")).WillRepeatedly(Return(nullptr));

  test::VerifyEventsEqual(&foo, model_->GetEvent(foo.name()));
  EXPECT_EQ(nullptr, model_->GetEvent("bar"));
}

TEST_F(InitAwareEventModelTest, PassThroughGetEventCount) {
  EXPECT_CALL(*mocked_model_, GetEventCount("foo", _, _))
      .WillRepeatedly(ReturnArg<1>());
  EXPECT_CALL(*mocked_model_, GetEventCount("bar", _, _))
      .WillRepeatedly(ReturnArg<2>());
  EXPECT_CALL(*mocked_model_, GetEventCount("qxz", _, _))
      .WillRepeatedly(Return(0));

  EXPECT_EQ(2u, model_->GetEventCount("foo", 2, 1));
  EXPECT_EQ(2u, model_->GetEventCount("foo", 2, 2));
  EXPECT_EQ(3u, model_->GetEventCount("foo", 3, 2));

  EXPECT_EQ(1u, model_->GetEventCount("bar", 2, 1));
  EXPECT_EQ(2u, model_->GetEventCount("bar", 2, 2));
  EXPECT_EQ(2u, model_->GetEventCount("bar", 3, 2));

  EXPECT_EQ(0u, model_->GetEventCount("qxz", 2, 1));
  EXPECT_EQ(0u, model_->GetEventCount("qxz", 2, 2));
  EXPECT_EQ(0u, model_->GetEventCount("qxz", 3, 2));
}

TEST_F(InitAwareEventModelTest, PassThroughIncrementEvent) {
  EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(true));

  Sequence sequence;
  EXPECT_CALL(*mocked_model_, IncrementEvent("foo", 0U)).InSequence(sequence);
  EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U)).InSequence(sequence);

  model_->IncrementEvent("foo", 0U);
  model_->IncrementEvent("bar", 1U);
  EXPECT_EQ(0U, model_->GetQueuedEventCountForTesting());
}

TEST_F(InitAwareEventModelTest, QueuedIncrementEvent) {
  {
    EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(false));
    EXPECT_CALL(*mocked_model_, IncrementEvent(_, _)).Times(0);

    model_->IncrementEvent("foo", 0U);
    model_->IncrementEvent("bar", 1U);
  }

  EventModel::OnModelInitializationFinished callback;
  EXPECT_CALL(*mocked_model_, Initialize(_, 2U))
      .WillOnce(
          [&callback](EventModel::OnModelInitializationFinished load_callback,
                      uint32_t current_day) {
            callback = std::move(load_callback);
          });
  model_->Initialize(std::move(load_callback_), 2U);

  {
    Sequence sequence;
    EXPECT_CALL(*mocked_model_, IncrementEvent("foo", 0U))
        .Times(1)
        .InSequence(sequence);
    EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U))
        .Times(1)
        .InSequence(sequence);

    std::move(callback).Run(true);
    EXPECT_TRUE(load_success_.value());
  }

  EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mocked_model_, IncrementEvent("qux", 3U)).Times(1);
  model_->IncrementEvent("qux", 3U);
  EXPECT_EQ(0U, model_->GetQueuedEventCountForTesting());
}

TEST_F(InitAwareEventModelTest, QueuedIncrementEventWithUnsuccessfulInit) {
  {
    EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(false));
    EXPECT_CALL(*mocked_model_, IncrementEvent(_, _)).Times(0);

    model_->IncrementEvent("foo", 0U);
    model_->IncrementEvent("bar", 1U);
  }

  EventModel::OnModelInitializationFinished callback;

  EXPECT_CALL(*mocked_model_, Initialize(_, 2U))
      .WillOnce(
          [&callback](EventModel::OnModelInitializationFinished load_callback,
                      uint32_t current_day) {
            callback = std::move(load_callback);
          });
  model_->Initialize(std::move(load_callback_), 2U);

  {
    Sequence sequence;
    EXPECT_CALL(*mocked_model_, IncrementEvent("foo", 0U))
        .Times(0)
        .InSequence(sequence);
    EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U))
        .Times(0)
        .InSequence(sequence);

    std::move(callback).Run(false);
    EXPECT_FALSE(load_success_.value());
    EXPECT_EQ(0U, model_->GetQueuedEventCountForTesting());
  }

  EXPECT_CALL(*mocked_model_, IncrementEvent("qux", 3U)).Times(0);
  model_->IncrementEvent("qux", 3U);
  EXPECT_EQ(0U, model_->GetQueuedEventCountForTesting());
}

}  // namespace feature_engagement
