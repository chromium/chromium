// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class MockBufferingControllerClient {
 public:
  MOCK_METHOD1(OnBufferingNotification, void(bool is_buffering));
};

}  // namespace

class BufferingControllerTest : public testing::Test {
 public:
  BufferingControllerTest();

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BufferingController> buffering_controller_;

  MockBufferingControllerClient client_;

  // Buffer level under the low level threshold.
  base::TimeDelta d1_;

  // Buffer level between the low and the high level.
  base::TimeDelta d2_;

  // Buffer level above the high level.
  base::TimeDelta d3_;
};

BufferingControllerTest::BufferingControllerTest() {
  base::TimeDelta low_level_threshold(
      base::TimeDelta::FromMilliseconds(2000));
  base::TimeDelta high_level_threshold(
      base::TimeDelta::FromMilliseconds(6000));

  d1_ = low_level_threshold - base::TimeDelta::FromMilliseconds(50);
  d2_ = (low_level_threshold + high_level_threshold) / 2;
  d3_ = high_level_threshold + base::TimeDelta::FromMilliseconds(50);

  scoped_refptr<BufferingConfig> buffering_config(
      new BufferingConfig(low_level_threshold, high_level_threshold));
  buffering_controller_.reset(new BufferingController(
      buffering_config,
      base::Bind(&MockBufferingControllerClient::OnBufferingNotification,
                 base::Unretained(&client_))));
}

TEST_F(BufferingControllerTest, OneStream_Typical) {
  EXPECT_CALL(client_, OnBufferingNotification(true)).Times(1);
  scoped_refptr<BufferingState> buffering_state =
      buffering_controller_->AddStream("test");
  buffering_state->SetMediaTime(base::TimeDelta());

  // Simulate pre-buffering.
  buffering_state->SetBufferedTime(d2_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kMediumLevel);

  EXPECT_CALL(client_, OnBufferingNotification(false)).Times(1);
  buffering_state->SetBufferedTime(d3_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kHighLevel);

  // Simulate some fluctuations of the buffering level.
  buffering_state->SetBufferedTime(d2_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kMediumLevel);

  // Simulate an underrun.
  EXPECT_CALL(client_, OnBufferingNotification(true)).Times(1);
  buffering_state->SetBufferedTime(d1_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kLowLevel);

  EXPECT_CALL(client_, OnBufferingNotification(false)).Times(1);
  buffering_state->SetBufferedTime(d3_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kHighLevel);

  // Simulate the end of stream.
  buffering_state->NotifyEos();
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kEosReached);

  buffering_state->SetBufferedTime(d2_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kEosReached);

  buffering_state->SetBufferedTime(d1_);
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kEosReached);
}

TEST_F(BufferingControllerTest, OneStream_LeaveBufferingOnEos) {
  EXPECT_CALL(client_, OnBufferingNotification(true)).Times(1);
  scoped_refptr<BufferingState> buffering_state =
      buffering_controller_->AddStream("test");
  buffering_state->SetMediaTime(base::TimeDelta());

  EXPECT_CALL(client_, OnBufferingNotification(false)).Times(1);
  buffering_state->NotifyEos();
  EXPECT_EQ(buffering_state->GetState(), BufferingState::kEosReached);
}

}  // namespace media
}  // namespace chromecast
