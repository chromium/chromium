// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/test_util.h"

#include <string_view>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

std::string TrimLines(std::string_view text) {
  auto lines = base::SplitString(text, "\n", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  return base::JoinString(lines, "\n");
}
void RunLoopUntil(base::RepeatingCallback<bool()> criteria,
                  const std::string& failure_message) {
  RunLoopUntil(criteria,
               base::BindLambdaForTesting([&]() { return failure_message; }));
}
void RunLoopUntil(base::RepeatingCallback<bool()> criteria,
                  base::OnceCallback<std::string()> failure_message_callback) {
  if (criteria.Run())
    return;
  constexpr int kMaxIterations = 1000;
  int iteration = 0;
  base::RunLoop run_loop;
  base::RepeatingClosure check_conditions_callback;
  auto schedule_check = [&]() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, check_conditions_callback);
  };
  check_conditions_callback = base::BindLambdaForTesting([&]() {
    if (criteria.Run() || ++iteration > kMaxIterations) {
      run_loop.QuitClosure().Run();
      ASSERT_LE(iteration, kMaxIterations)
          << "RunLoopUntil criteria still not true after max iteration count:"
          << std::move(failure_message_callback).Run();
    } else {
      schedule_check();
    }
  });
  schedule_check();
  run_loop.Run();
}

}  // namespace feed
