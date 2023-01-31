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
  void UpdateWindowControlsOverlay(
      const gfx::Rect& bounding_rect) const override {}
  bool IsTranslucentWindowOpacitySupported() const override { return true; }
  bool ShouldDrawRestoredFrameShadow() const override { return true; }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  ui::WindowTiledEdges GetTiledEdges() const override { return tiled_edges_; }
#endif
  int WebAppButtonHeight() const override { return 0; }

  ui::WindowTiledEdges tiled_edges_;
};

}  // namespace

using BrowserFrameViewLayoutLinuxTest = ChromeViewsTestBase;

// Tests that frame border insets are set to zero for the tiled edges.
TEST_F(BrowserFrameViewLayoutLinuxTest, FrameInsets) {
  BrowserFrameViewLayoutLinux layout;
  TestLayoutDelegate delegate;
  layout.set_delegate(&delegate);

  for (const bool left : {false, true}) {
    for (const bool right : {false, true}) {
      for (const bool top : {false, true}) {
        for (const bool bottom : {false, true}) {
          delegate.tiled_edges_ = {
              .left = left, .right = right, .top = top, .bottom = bottom};
          const auto normal_insets = layout.MirroredFrameBorderInsets();
          if (left)
            EXPECT_EQ(normal_insets.left(), 0);
          else
            EXPECT_GT(normal_insets.left(), 0);
          if (right)
            EXPECT_EQ(normal_insets.right(), 0);
          else
            EXPECT_GT(normal_insets.right(), 0);
          if (top) {
            EXPECT_EQ(normal_insets.top(), 0);
            EXPECT_EQ(layout.FrameTopBorderThickness(true), 0);
          } else {
            EXPECT_GT(normal_insets.top(), 0);
            EXPECT_GT(layout.FrameTopBorderThickness(true), 0);
          }
          if (bottom)
            EXPECT_EQ(normal_insets.bottom(), 0);
          else
            EXPECT_GT(normal_insets.bottom(), 0);
        }
      }
    }
  }
}
