// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_icon_finder.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"

// The most simple test possible to ensure that the finder doesn't leak or
// cause crashes.
TEST(TaskbarIconFinder, Simple) {
  base::test::TaskEnvironment task_environment;
  base::RunLoop run_loop;

  // Extend the timeout since the use of UI automation can take some time on a
  // machine under heavy load (e.g., one that is running many tests in
  // parallel).
  base::test::ScopedRunLoopTimeout scoped_run_loop_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout(), base::BindRepeating([]() {
        return std::string(
            "FindTaskbarIcon did not complete in a timely fashion.");
      }));
  FindTaskbarIcon(
      base::BindOnce([](base::OnceClosure quit,
                        const gfx::Rect& rect) { std::move(quit).Run(); },
                     run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
}
