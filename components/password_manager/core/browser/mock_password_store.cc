// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_password_store.h"

#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace password_manager {

MockPasswordStore::MockPasswordStore() = default;

MockPasswordStore::~MockPasswordStore() = default;

scoped_refptr<base::SequencedTaskRunner>
MockPasswordStore::CreateBackgroundTaskRunner() const {
  return base::SequencedTaskRunnerHandle::Get();
}

bool MockPasswordStore::InitOnBackgroundSequence() {
  return true;
}

}  // namespace password_manager
