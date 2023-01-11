// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "base/functional/bind.h"

namespace offline_pages {

RequestQueueTaskTestBase::RequestQueueTaskTestBase()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_current_default_handle_(task_runner_) {}

RequestQueueTaskTestBase::~RequestQueueTaskTestBase() = default;

void RequestQueueTaskTestBase::TearDown() {
  store_.Close();
  PumpLoop();
}

void RequestQueueTaskTestBase::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueTaskTestBase::InitializeStore() {
  store_.Initialize(base::BindOnce([](bool success) { ASSERT_TRUE(success); }));
  PumpLoop();
}

}  // namespace offline_pages
