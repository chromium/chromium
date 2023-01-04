// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_TASK_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_TASK_TEST_BASE_H_

#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class RequestQueueTaskTestBase : public testing::Test {
 public:
  RequestQueueTaskTestBase();
  ~RequestQueueTaskTestBase() override;

  void TearDown() override;
  void PumpLoop();

  void InitializeStore();

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() {
    return task_runner_;
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  TestRequestQueueStore store_;
};
}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_TASK_TEST_BASE_H_
