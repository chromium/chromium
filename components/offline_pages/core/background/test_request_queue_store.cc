// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/test_request_queue_store.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_pages/core/background/request_queue_store.h"

namespace offline_pages {

TestRequestQueueStore::TestRequestQueueStore()
    : RequestQueueStore(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

TestRequestQueueStore::~TestRequestQueueStore() {
  // Delete the database and run all tasks.
  SetStateForTesting(StoreState::NOT_LOADED, true);
}

void TestRequestQueueStore::Close() {
  SetStateForTesting(StoreState::NOT_LOADED, true);
}

void TestRequestQueueStore::set_force_initialize_fail() {
  force_initialize_fail_ = true;
}

bool TestRequestQueueStore::force_initialize_fail() {
  return force_initialize_fail_;
}

void TestRequestQueueStore::set_resume_after_reset() {
  resume_after_reset_ = true;
}

void TestRequestQueueStore::Initialize(InitializeCallback callback) {
  if (force_initialize_fail_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    RequestQueueStore::Initialize(std::move(callback));
  }
}

void TestRequestQueueStore::Reset(ResetCallback callback) {
  if (force_initialize_fail_ && resume_after_reset_) {
    force_initialize_fail_ = false;
    SetStateForTesting(StoreState::NOT_LOADED, false);
  } else if (force_initialize_fail_) {
    SetStateForTesting(StoreState::FAILED_RESET, false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  RequestQueueStore::Reset(std::move(callback));
}

}  // namespace offline_pages
