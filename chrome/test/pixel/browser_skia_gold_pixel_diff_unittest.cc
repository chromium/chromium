// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "chrome/test/base/test_browser_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::Return;

class MockBrowserSkiaGoldPixelDiff : public BrowserSkiaGoldPixelDiff {
 public:
  MockBrowserSkiaGoldPixelDiff() {}
  MOCK_CONST_METHOD2(UploadToSkiaGoldServer,
                     bool(const base::FilePath&, const std::string&));
  bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image) const {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    *image = gfx::Image::CreateFrom1xBitmap(bitmap);
    return true;
  }
  int LaunchProcess(const base::CommandLine& cmdline) const override {
    return 0;
  }
};

class BrowserSkiaGoldPixelDiffTest : public views::test::WidgetTest {
 public:
  BrowserSkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("build-revision", "test");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSkiaGoldPixelDiffTest);
};

TEST_F(BrowserSkiaGoldPixelDiffTest, CompareScreenshotByView) {
  views::View view;
  MockBrowserSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, UploadToSkiaGoldServer(_, "Prefix_Demo"))
      .Times(1)
      .WillOnce(Return(true));
  views::Widget* widget = CreateTopLevelNativeWidget();
  mock_pixel.Init(widget, "Prefix");
  bool ret = mock_pixel.CompareScreenshot("Demo", &view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}
