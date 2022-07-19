// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>
#include <string>

#include "ash/webui/projector_app/projector_screencast.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kScreencastId[] = "screencastId";

}  // namespace

class ScreencastManagerTest : public testing::Test {
 protected:
  ScreencastManager& screencast_manager() { return screencast_manager_; }

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  ScreencastManager screencast_manager_;
};

TEST_F(ScreencastManagerTest, GetScreencast) {
  base::RunLoop run_loop;
  screencast_manager().GetScreencast(
      kScreencastId,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ash::ProjectorScreencast> screencast,
                      const std::string& error) {
            EXPECT_EQ(screencast->container_folder_id, kScreencastId);
            EXPECT_TRUE(error.empty());
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace ash
