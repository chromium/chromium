// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"
#include "base/files/file_path.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class ArcGraphicsTracingHandlerTest : public testing::Test {
 public:
  ArcGraphicsTracingHandlerTest() = default;
  ~ArcGraphicsTracingHandlerTest() override = default;

  ArcGraphicsTracingHandlerTest(const ArcGraphicsTracingHandlerTest&) = delete;
  ArcGraphicsTracingHandlerTest& operator=(
      const ArcGraphicsTracingHandlerTest&) = delete;

 protected:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

TEST_F(ArcGraphicsTracingHandlerTest, ModelName) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return base::Time::UnixEpoch() + base::Seconds(1); }, nullptr,
      nullptr);

  const base::FilePath download_path =
      file_manager::util::GetDownloadsFolderForProfile(profile());
  EXPECT_EQ(download_path.AppendASCII(
                "overview_tracing_test_title_1_11644473601.json"),
            ArcGraphicsTracingHandler::GetModelPathFromTitle(profile(),
                                                             "Test Title #:1"));
  EXPECT_EQ(
      download_path.AppendASCII(
          "overview_tracing_0123456789012345678901234567890_11644473601.json"),
      ArcGraphicsTracingHandler::GetModelPathFromTitle(
          profile(), "0123456789012345678901234567890123456789"));
}

}  // namespace

}  // namespace ash
