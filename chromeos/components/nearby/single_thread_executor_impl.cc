// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/single_thread_executor_impl.h"

namespace chromeos {

namespace nearby {

SingleThreadExecutorImpl::SingleThreadExecutorImpl()
    : SubmittableExecutorBase(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})) {}

SingleThreadExecutorImpl::~SingleThreadExecutorImpl() = default;

void SingleThreadExecutorImpl::shutdown() {
  Shutdown();
}

void SingleThreadExecutorImpl::execute(
    std::shared_ptr<location::nearby::Runnable> runnable) {
  Execute(runnable);
}

}  // namespace nearby

}  // namespace chromeos
