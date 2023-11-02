// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/test_support_callbacks.h"

#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace reporting {
namespace test {

TestCallbackWaiter::TestCallbackWaiter()
    : signaled_cb_(
          base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                             base::test::TestFuture<bool>::GetCallback())) {}
TestCallbackWaiter::~TestCallbackWaiter() = default;

TestCallbackAutoWaiter::TestCallbackAutoWaiter() {
  Attach();
}
TestCallbackAutoWaiter::~TestCallbackAutoWaiter() {
  Wait();
}

}  // namespace test
}  // namespace reporting
