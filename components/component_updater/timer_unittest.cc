// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/component_updater/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace component_updater {

class ComponentUpdaterTimerTest : public testing::Test {
 public:
  ComponentUpdaterTimerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~ComponentUpdaterTimerTest() override {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(ComponentUpdaterTimerTest, Start) {
  class TimerClientMock {
   public:
    TimerClientMock(int max_count, base::OnceClosure quit_closure)
        : max_count_(max_count),
          quit_closure_(std::move(quit_closure)),
          count_(0) {}

    void OnTimerEvent() {
      ++count_;
      if (count_ >= max_count_)
        std::move(quit_closure_).Run();
    }

    int count() const { return count_; }

   private:
    const int max_count_;
    base::OnceClosure quit_closure_;

    int count_;
  };

  base::RunLoop run_loop;
  TimerClientMock timer_client_fake(3, run_loop.QuitClosure());
  EXPECT_EQ(0, timer_client_fake.count());

  Timer timer;
  const base::TimeDelta delay(base::TimeDelta::FromMilliseconds(1));
  timer.Start(delay, delay,
              base::BindRepeating(&TimerClientMock::OnTimerEvent,
                                  base::Unretained(&timer_client_fake)));
  run_loop.Run();
  timer.Stop();

  EXPECT_EQ(3, timer_client_fake.count());
}

}  // namespace component_updater
