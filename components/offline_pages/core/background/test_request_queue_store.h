// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_TEST_REQUEST_QUEUE_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_TEST_REQUEST_QUEUE_STORE_H_

#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/background/request_queue_store.h"

namespace offline_pages {

// Wraps RequestQueueStore for easy use in tests. Stores data in memory and
// provides additional testing functionality.
class TestRequestQueueStore : public RequestQueueStore {
 public:
  TestRequestQueueStore();
  ~TestRequestQueueStore() override;

  void Close();

  // RequestQueueStore implementation.

  void Initialize(InitializeCallback callback) override;
  void Reset(ResetCallback callback) override;

  // Test functionality.

  // Forces initialization failure on the database.
  void set_force_initialize_fail();
  bool force_initialize_fail();

  // In conjunction with set_force_initialize_fail, this allows the database
  // to initialize after Reset() is called.
  void set_resume_after_reset();

 private:
  bool resume_after_reset_ = false;
  bool force_initialize_fail_ = false;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_TEST_REQUEST_QUEUE_STORE_H_
