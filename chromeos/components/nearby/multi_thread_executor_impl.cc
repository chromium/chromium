// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/multi_thread_executor_impl.h"

namespace chromeos {

namespace nearby {

MultiThreadExecutorImpl::MultiThreadExecutorImpl()
    : SubmittableExecutorBase(
          base::CreateTaskRunner({base::ThreadPool(), base::MayBlock()})) {}

MultiThreadExecutorImpl::~MultiThreadExecutorImpl() = default;

void MultiThreadExecutorImpl::shutdown() {
  Shutdown();
}

void MultiThreadExecutorImpl::execute(
    std::shared_ptr<location::nearby::Runnable> runnable) {
  Execute(runnable);
}

}  // namespace nearby

}  // namespace chromeos
