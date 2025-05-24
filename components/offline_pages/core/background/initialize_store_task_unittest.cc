// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/initialize_store_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

class InitializeStoreTaskTest : public RequestQueueTaskTestBase {
 public:
  InitializeStoreTaskTest() : callback_called_(false), success_(false) {}
  ~InitializeStoreTaskTest() override = default;

  void InitializeCallback(bool success) {
    callback_called_ = true;
    success_ = success;
  }

  bool callback_called() const { return callback_called_; }

  bool last_call_successful() const { return success_; }

 private:
  bool callback_called_;
  bool success_;
};


TEST_F(InitializeStoreTaskTest, SuccessfulInitialization) {
  InitializeStoreTask task(
      &store_, base::BindOnce(&InitializeStoreTaskTest::InitializeCallback,
                              base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(last_call_successful());
  EXPECT_EQ(StoreState::LOADED, store_.state());
}

TEST_F(InitializeStoreTaskTest, SuccessfulReset) {
  store_.set_force_initialize_fail();
  store_.set_resume_after_reset();
  InitializeStoreTask task(
      &store_, base::BindOnce(&InitializeStoreTaskTest::InitializeCallback,
                              base::Unretained(this)));
  task.Execute(base::DoNothing());

  PumpLoop();

  // Reset should have cleared this value.
  EXPECT_TRUE(!store_.force_initialize_fail());
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(last_call_successful());

  EXPECT_EQ(StoreState::LOADED, store_.state());
}

TEST_F(InitializeStoreTaskTest, FailedReset) {
  store_.set_force_initialize_fail();
  InitializeStoreTask task(
      &store_, base::BindOnce(&InitializeStoreTaskTest::InitializeCallback,
                              base::Unretained(this)));
  task.Execute(base::DoNothing());
  PumpLoop();
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(last_call_successful());
  EXPECT_EQ(StoreState::FAILED_RESET, store_.state());
}

}  // namespace
}  // namespace offline_pages
