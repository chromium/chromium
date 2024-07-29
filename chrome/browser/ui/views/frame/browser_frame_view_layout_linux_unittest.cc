// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"

namespace {

class TestLayoutDelegate : public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  TestLayoutDelegate() = default;

  TestLayoutDelegate(const TestLayoutDelegate&) = delete;
  TestLayoutDelegate& operator=(const TestLayoutDelegate&) = delete;

  ~TestLayoutDelegate() override = default;

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override { return false; }
  bool ShouldShowWindowTitle() const override { return false; }
  std::u16string GetWindowTitle() const override { return std::u16string(); }
  int GetIconSize() const override { return 17; }
  gfx::Size GetBrowserViewMinimumSize() const override { return {168, 64}; }
  bool ShouldShowCaptionButtons() const override { return true; }
  bool IsRegularOrGuestSession() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  bool IsMaximized() const override { return false; }
  bool IsMinimized() const override { return false; }
  bool IsFullscreen() const override { return false; }
  bool IsTabStripVisible() const override { return true; }
  bool GetBorderlessModeEnabled() const override { return false; }
  int GetTabStripHeight() const override {
    return GetLayoutConstant(TAB_HEIGHT);
  }
  bool IsToolbarVisible() const override { return true; }
  gfx::Size GetTabstripMinimumSize() const override { return {78, 29}; }
  int GetTopAreaHeight() const override { return 0; }
  bool UseCustomFrame() const override { return true; }
  bool IsFrameCondensed() const override { return false; }
  bool EverHasVisibleBackgroundTabShapes() const override { return false; }
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override {}
  bool ShouldDrawRestoredFrameShadow() const override { return true; }
#if BUILDFLAG(IS_LINUX)
  bool IsTiled() const override { return tiled_; }
#endif
  int WebAppButtonHeight() const override { return 0; }

  bool tiled_;
};

}  // namespace

using BrowserFrameViewLayoutLinuxTest = ChromeViewsTestBase;

// Tests that frame border insets are set to zero for the tiled edges.
TEST_F(BrowserFrameViewLayoutLinuxTest, FrameInsets) {
  BrowserFrameViewLayoutLinux layout;
  TestLayoutDelegate delegate;
  layout.set_delegate(&delegate);
  auto input_insets = layout.GetInputInsets();

  for (const bool tiled : {false, true}) {
    delegate.tiled_ = tiled;
    const auto normal_insets = layout.RestoredMirroredFrameBorderInsets();
    if (tiled) {
      EXPECT_EQ(normal_insets.left(), input_insets.left());
      EXPECT_EQ(normal_insets.right(), input_insets.right());
      EXPECT_EQ(normal_insets.top(), input_insets.top());
      EXPECT_EQ(normal_insets.bottom(), input_insets.bottom());
    } else {
      EXPECT_GE(normal_insets.left(), input_insets.left());
      EXPECT_GE(normal_insets.right(), input_insets.right());
      EXPECT_GE(normal_insets.top(), input_insets.top());
      EXPECT_GE(normal_insets.bottom(), input_insets.bottom());
    }
  }
}
