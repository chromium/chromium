// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/callback_delayer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;

}  // namespace

class CallbackDelayerTest : public ::testing::Test {
 public:
 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::RepeatingCallback<void(double)> CreateProgressCallback() {
    return base::BindLambdaForTesting([&](double progress) {
      progress_update_count_++;
      last_progress_ = progress;
    });
  }

  int progress_update_count() { return progress_update_count_; }
  double last_progress() { return last_progress_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int progress_update_count_ = 0;
  double last_progress_ = 0;
};

TEST_F(CallbackDelayerTest, ProgressCallbackCalled) {
  base::test::TestFuture<void> complete;
  CallbackDelayer delayer(base::Seconds(2), 0.8, CreateProgressCallback());
  delayer.StartDelayingCallback(complete.GetCallback()).Run();

  FastForwardBy(base::Seconds(1));

  EXPECT_THAT(progress_update_count(), Eq(60));
  EXPECT_THAT(last_progress(), DoubleEq(0.5));
}

TEST_F(CallbackDelayerTest, CallbackDelayed) {
  base::test::TestFuture<void> complete;
  CallbackDelayer delayer(base::Seconds(2), 0.8, CreateProgressCallback());
  base::OnceClosure callback =
      delayer.StartDelayingCallback(complete.GetCallback());

  std::move(callback).Run();
  EXPECT_THAT(complete.IsReady(), IsFalse());

  FastForwardBy(base::Seconds(1));
  EXPECT_THAT(complete.IsReady(), IsFalse());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(complete.IsReady(), IsTrue());
}

TEST_F(CallbackDelayerTest, ProgressPausesIfCallbackNotCalled) {
  base::test::TestFuture<void> complete;
  CallbackDelayer delayer(base::Seconds(2), 0.8, CreateProgressCallback());
  base::OnceClosure callback =
      delayer.StartDelayingCallback(complete.GetCallback());

  FastForwardBy(base::Seconds(3));

  EXPECT_THAT(complete.IsReady(), IsFalse());
  EXPECT_THAT(last_progress(), DoubleEq(0.8));

  int pre_pause_progress_update_count = progress_update_count();
  std::move(callback).Run();
  FastForwardBy(base::Seconds(1));

  EXPECT_THAT(complete.IsReady(), IsTrue());
  EXPECT_THAT(progress_update_count() - pre_pause_progress_update_count,
              Eq(24));  // 20% of 2s at 60hz
  EXPECT_THAT(last_progress(), DoubleEq(1.0));
}

}  // namespace web_app
