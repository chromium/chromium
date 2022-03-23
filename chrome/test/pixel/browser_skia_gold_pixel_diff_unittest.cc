// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "chrome/test/base/test_browser_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::Return;

class MockBrowserSkiaGoldPixelDiff : public BrowserSkiaGoldPixelDiff {
 public:
  MockBrowserSkiaGoldPixelDiff() = default;
  MOCK_CONST_METHOD1(LaunchProcess, int(const base::CommandLine&));
  bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image) const override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    *image = gfx::Image::CreateFrom1xBitmap(bitmap);
    return true;
  }
};

class MockBrowserSkiaGoldPixelDiffMockUpload
    : public MockBrowserSkiaGoldPixelDiff {
 public:
  MockBrowserSkiaGoldPixelDiffMockUpload() = default;
  MOCK_CONST_METHOD3(UploadToSkiaGoldServer,
                     bool(const base::FilePath&,
                          const std::string&,
                          const ui::test::SkiaGoldMatchingAlgorithm*));
};

class BrowserSkiaGoldPixelDiffTest : public views::test::WidgetTest {
 public:
  BrowserSkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("git-revision", "test");
  }

  BrowserSkiaGoldPixelDiffTest(const BrowserSkiaGoldPixelDiffTest&) = delete;
  BrowserSkiaGoldPixelDiffTest& operator=(const BrowserSkiaGoldPixelDiffTest&) =
      delete;
};

TEST_F(BrowserSkiaGoldPixelDiffTest, CompareScreenshotByView) {
  views::View view;
  MockBrowserSkiaGoldPixelDiffMockUpload mock_pixel;
  EXPECT_CALL(
      mock_pixel,
      UploadToSkiaGoldServer(
          _, "Prefix_Demo_" + ui::test::SkiaGoldPixelDiff::GetPlatform(), _))
      .Times(1)
      .WillOnce(Return(true));
  views::Widget* widget = CreateTopLevelNativeWidget();
  mock_pixel.Init(widget, "Prefix");
  bool ret = mock_pixel.CompareScreenshot("Demo", &view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}

TEST_F(BrowserSkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  views::View view;
  MockBrowserSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(0);
  views::Widget* widget = CreateTopLevelNativeWidget();
  mock_pixel.Init(widget, "Prefix");
  bool ret = mock_pixel.CompareScreenshot("Demo", &view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}
