// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/timed_callback.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {

TEST(TimedCallbackTest, CallNoTimeout) {
  base::test::TaskEnvironment task_environment;
  base::MockOnceClosure mock;
  EXPECT_CALL(mock, Run());
  std::move(MakeTimedCallback(mock.Get(), base::Seconds(55))).Run();
}

TEST(TimedCallbackTest, TimeoutThenCall) {
  base::test::TaskEnvironment task_environment;
  base::MockOnceCallback<void(int)> mock;
  base::RunLoop loop;
  EXPECT_CALL(mock, Run(1));
  base::OnceCallback<void(int)> callback = MakeTimedCallback(
      base::BindOnce(mock.Get().Then(loop.QuitClosure())), base::Seconds(1), 1);
  loop.Run();
  std::move(callback).Run(2);  // Should have no effect.
}

TEST(TimedCallbackTest, CallThenTimeout) {
  base::test::TaskEnvironment task_environment;
  base::MockOnceClosure mock;
  base::RunLoop loop;
  EXPECT_CALL(mock, Run());
  MakeTimedCallback(mock.Get(), base::Seconds(0)).Run();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(10));
  loop.Run();
}

TEST(TimedCallbackTest, MoveOnlyParameter) {
  base::test::TaskEnvironment task_environment;
  base::MockOnceClosure mock;
  EXPECT_CALL(mock, Run());
  std::move(MakeTimedCallback(base::BindOnce([](base::OnceClosure closure) {
                                std::move(closure).Run();
                              }),
                              base::Seconds(55), base::BindOnce([] {})))
      .Run(mock.Get());
}

}  // namespace
}  // namespace update_client
