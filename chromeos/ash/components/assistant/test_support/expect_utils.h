// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ASSISTANT_TEST_SUPPORT_EXPECT_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_ASSISTANT_TEST_SUPPORT_EXPECT_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace assistant {
namespace test {

namespace {

template <typename T>
void CheckResult(base::OnceClosure quit,
                 T expected_value,
                 base::RepeatingCallback<T()> value_callback) {
  if (expected_value == value_callback.Run()) {
    std::move(quit).Run();
    return;
  }

  // Check again in the future
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(CheckResult<T>, std::move(quit), expected_value,
                     value_callback),
      base::Milliseconds(10));
}

}  // namespace

// Check if the |expected_value| is equal to the result of running
// |value_callback|.  This method will block and continuously try the
// comparison above until it succeeds, or timeout.
template <typename T>
void ExpectResult(T expected_value,
                  base::RepeatingCallback<T()> value_callback,
                  const std::string& tag = std::string()) {
  // Wait until we're ready or we hit the default task environment timeout.
  base::RunLoop run_loop;
  CheckResult(run_loop.QuitClosure(), expected_value, value_callback);

  EXPECT_NO_FATAL_FAILURE(run_loop.Run())
      << (tag.empty() ? tag : tag + ": ")
      << "Failed waiting for expected result.\n"
      << "Expected \"" << expected_value << "\"\n"
      << "Got \"" << value_callback.Run() << "\"";
}

#define WAIT_FOR_CALL(mock_obj, call)                                        \
  {                                                                          \
    base::RunLoop run_loop;                                                  \
    EXPECT_CALL(mock_obj, call).WillOnce([quit = run_loop.QuitClosure()]() { \
      std::move(quit).Run();                                                 \
    });                                                                      \
    EXPECT_NO_FATAL_FAILURE(run_loop.Run())                                  \
        << #mock_obj << "::" << #call << " is NOT called.";                  \
  }

}  // namespace test
}  // namespace assistant
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ASSISTANT_TEST_SUPPORT_EXPECT_UTILS_H_
