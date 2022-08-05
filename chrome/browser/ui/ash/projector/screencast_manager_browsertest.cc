// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>

#include "ash/webui/projector_app/projector_screencast.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kVideoFileId[] = "videoFileId";
constexpr char kResourceKey[] = "resourceKey";

}  // namespace

class ScreencastManagerTest : public InProcessBrowserTest {
 protected:
  ScreencastManager& screencast_manager() { return screencast_manager_; }

 private:
  ScreencastManager screencast_manager_;
};

IN_PROC_BROWSER_TEST_F(ScreencastManagerTest, GetVideoSuccess) {
  base::RunLoop run_loop;
  screencast_manager().GetVideo(
      kVideoFileId, kResourceKey,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(video->file_id, kVideoFileId);
            EXPECT_TRUE(error_message.empty());
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace ash
