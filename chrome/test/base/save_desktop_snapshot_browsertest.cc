// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "save_desktop_snapshot.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

class SaveDesktopSnapshotTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(kSnapshotOutputDir, temp_dir_.GetPath());
  }

  void TearDownInProcessBrowserTestFixture() override {
    ASSERT_TRUE(temp_dir_.Delete());
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(SaveDesktopSnapshotTest, SaveDesktopSnapshot) {
  base::ScopedAllowBlockingForTesting allow_io;
  auto snapshot_path = SaveDesktopSnapshot();

#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    // DesktopCapturer is not well supported for wayland.
    // SaveDesktopSnapshot() should return a empty file path instead of crash.
    ASSERT_TRUE(snapshot_path.empty());
    return;
  }
#endif

  ASSERT_FALSE(snapshot_path.empty());
  ASSERT_TRUE(base::PathExists(snapshot_path));
}
