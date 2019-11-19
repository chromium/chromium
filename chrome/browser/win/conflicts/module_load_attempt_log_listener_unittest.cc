// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_load_attempt_log_listener.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_elf/sha1/sha1.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ModuleLoadAttemptLogListenerTest : public testing::Test {
 protected:
  ModuleLoadAttemptLogListenerTest() = default;
  ~ModuleLoadAttemptLogListenerTest() override = default;

  std::unique_ptr<ModuleLoadAttemptLogListener>
  CreateModuleLoadAttemptLogListener() {
    return std::make_unique<ModuleLoadAttemptLogListener>(
        base::BindRepeating(&ModuleLoadAttemptLogListenerTest::OnModuleBlocked,
                            base::Unretained(this)));
  }

  // ModuleLoadAttemptLogListener::Delegate:
  void OnModuleBlocked(const base::FilePath& module_path,
                       uint32_t module_size,
                       uint32_t module_time_date_stamp) {
    blocked_modules_.emplace_back(module_path, module_size,
                                  module_time_date_stamp);

    notified_ = true;

    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  void WaitForNotification() {
    if (notified_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>>&
  blocked_modules() {
    return blocked_modules_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  bool notified_ = false;

  base::Closure quit_closure_;

  std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>> blocked_modules_;

  DISALLOW_COPY_AND_ASSIGN(ModuleLoadAttemptLogListenerTest);
};

}  // namespace

TEST_F(ModuleLoadAttemptLogListenerTest, DrainLog) {
  auto module_load_attempt_log_listener = CreateModuleLoadAttemptLogListener();

  WaitForNotification();

  // Only the blocked entry is returned.
  ASSERT_EQ(1u, blocked_modules().size());
}
