// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/initialize_store_task.h"

#include "base/bind.h"

namespace offline_pages {

const int kRestartAttemptsMaximum = 3;

InitializeStoreTask::InitializeStoreTask(
    RequestQueueStore* store,
    RequestQueueStore::InitializeCallback callback)
    : store_(store),
      reset_attempts_left_(kRestartAttemptsMaximum),
      callback_(std::move(callback)) {}

InitializeStoreTask::~InitializeStoreTask() {}

void InitializeStoreTask::Run() {
  InitializeStore();
}

void InitializeStoreTask::InitializeStore() {
  store_->Initialize(base::BindOnce(&InitializeStoreTask::CompleteIfSuccessful,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void InitializeStoreTask::CompleteIfSuccessful(bool success) {
  if (success) {
    std::move(callback_).Run(true);
    TaskComplete();
    return;
  }

  TryToResetStore();
}

void InitializeStoreTask::TryToResetStore() {
  if (reset_attempts_left_ == 0) {
    std::move(callback_).Run(false);
    TaskComplete();
    return;
  }

  reset_attempts_left_--;
  store_->Reset(base::BindOnce(&InitializeStoreTask::OnStoreResetDone,
                               weak_ptr_factory_.GetWeakPtr()));
}

void InitializeStoreTask::OnStoreResetDone(bool success) {
  if (success)
    InitializeStore();
  else
    TryToResetStore();
}

}  // namespace offline_pages
