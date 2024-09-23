// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"

namespace {

std::optional<int> FindNonTrivialFactor(int n) {
  // Really naive algorithm.
  for (int i = 2; i < n; ++i) {
    if (n % i == 0) {
      return i;
    }
  }
  return std::nullopt;
}

void FindNonTrivialFactorHelper(int n, std::optional<int>* result) {
  *result = FindNonTrivialFactor(n);
}

void PrintStatusUpdate(base::TimeTicks start_time) {
  double num_seconds = (base::TimeTicks::Now() - start_time).InSecondsF();
  LOG(INFO) << "Waited for " << num_seconds << " seconds...\n";
}

void PrintStatusUpdateRepeatedly(base::TimeTicks start_time,
                                 base::TimeDelta print_interval) {
  PrintStatusUpdate(start_time);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintStatusUpdateRepeatedly, start_time, print_interval),
      print_interval);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};

  if (argc < 2) {
    LOG(INFO) << argv[0] << ": missing operand\n";
    return -1;
  }

  int n = 0;
  if (!base::StringToInt(argv[1], &n) || n < 2) {
    LOG(INFO) << argv[0] << ": invalid n '" << argv[1] << "'";
    return -1;
  }

  base::RunLoop run_loop;

  std::optional<int> result;
  // Notice that we're posting the long-running factoring operation to
  // `base::ThreadPool` to avoid blocking the main thread.
  //
  // `run_loop.QuitClosure()` will be called once the factoring task completes.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce(&FindNonTrivialFactorHelper, n, &result),
      run_loop.QuitClosure());

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&PrintStatusUpdateRepeatedly,
                                base::TimeTicks::Now(), base::Seconds(1)));

  run_loop.Run();

  if (result.has_value()) {
    LOG(INFO) << "found non-trivial factor " << result.value() << " for " << n;
    DCHECK_EQ(n % result.value(), 0);
  } else {
    LOG(INFO) << n << " is prime";
  }

  return 0;
}
