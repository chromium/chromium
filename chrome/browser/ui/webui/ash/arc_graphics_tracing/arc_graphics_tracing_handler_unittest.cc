// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestHandler : public ArcGraphicsTracingHandler {
 public:
  void set_downloads_folder(const base::FilePath& downloads_folder) {
    downloads_folder_ = downloads_folder;
  }

  void set_now(base::Time now) { now_ = now; }

 private:
  base::FilePath GetDownloadsFolder() override { return downloads_folder_; }
  base::Time Now() override { return now_; }

  base::FilePath downloads_folder_;
  base::Time now_;
};

class ArcGraphicsTracingHandlerTest : public ChromeAshTestBase {
 public:
  ArcGraphicsTracingHandlerTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

  ~ArcGraphicsTracingHandlerTest() override = default;

  ArcGraphicsTracingHandlerTest(const ArcGraphicsTracingHandlerTest&) = delete;
  ArcGraphicsTracingHandlerTest& operator=(
      const ArcGraphicsTracingHandlerTest&) = delete;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    // WMHelper constructor sets a global instance which the Handler constructor
    // requires.
    wm_helper_ = std::make_unique<exo::WMHelper>();
    handler_ = std::make_unique<TestHandler>();
  }

  void TearDown() override {
    handler_.reset();
    wm_helper_.reset();

    ChromeAshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<TestHandler> handler_;
};

TEST_F(ArcGraphicsTracingHandlerTest, ModelName) {
  base::FilePath download_path = base::FilePath::FromASCII("/mnt/downloads");
  handler_->set_downloads_folder(download_path);

  handler_->set_now(base::Time::UnixEpoch() + base::Seconds(1));
  EXPECT_EQ(download_path.AppendASCII(
                "overview_tracing_test_title_1_11644473601.json"),
            handler_->GetModelPathFromTitle("Test Title #:1"));
  EXPECT_EQ(
      download_path.AppendASCII(
          "overview_tracing_0123456789012345678901234567890_11644473601.json"),
      handler_->GetModelPathFromTitle(
          "0123456789012345678901234567890123456789"));

  handler_->set_now(base::Time::UnixEpoch() + base::Days(50));
  EXPECT_EQ(
      download_path.AppendASCII("overview_tracing_xyztitle_11648793600.json"),
      handler_->GetModelPathFromTitle("xyztitle"));

  download_path = base::FilePath::FromASCII("/var/DownloadFolder");
  handler_->set_downloads_folder(download_path);
  EXPECT_EQ(
      download_path.AppendASCII("overview_tracing_secret_app_11648793600.json"),
      handler_->GetModelPathFromTitle("Secret App"));
}

}  // namespace

}  // namespace ash
