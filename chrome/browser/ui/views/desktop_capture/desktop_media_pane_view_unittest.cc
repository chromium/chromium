// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_pane_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

class DesktopMediaPaneViewTest : public ::testing::Test {
 public:
  ~DesktopMediaPaneViewTest() override = default;

 protected:
  // Makes ChromeLayoutProvider available through the static
  // ChromeLayoutProvider::Get() accessor, which is used in
  // DesktopMediaPaneView.
  ChromeLayoutProvider layout_provider_;

  std::unique_ptr<DesktopMediaPaneView> pane_view_ =
      std::make_unique<DesktopMediaPaneView>(DesktopMediaList::Type::kScreen,
                                             std::make_unique<views::View>(),
                                             nullptr);
};

TEST_F(DesktopMediaPaneViewTest, TestDefaultState) {
  EXPECT_FALSE(pane_view_->IsPermissionPaneVisible());
  EXPECT_TRUE(pane_view_->IsContentPaneVisible());
}

// ScreenCapturePermissionChecker is only active on Mac.
#if BUILDFLAG(IS_MAC)
TEST_F(DesktopMediaPaneViewTest, TestWithPermission) {
  pane_view_->OnScreenCapturePermissionUpdate(/*has_permission=*/true);

  EXPECT_FALSE(pane_view_->IsPermissionPaneVisible());
  EXPECT_TRUE(pane_view_->IsContentPaneVisible());
}

TEST_F(DesktopMediaPaneViewTest, TestWithoutPermission) {
  pane_view_->OnScreenCapturePermissionUpdate(/*has_permission=*/false);

  EXPECT_TRUE(pane_view_->IsPermissionPaneVisible());
  EXPECT_FALSE(pane_view_->IsContentPaneVisible());
}

TEST_F(DesktopMediaPaneViewTest, TestPermissionEnabled) {
  pane_view_->OnScreenCapturePermissionUpdate(/*has_permission=*/false);
  pane_view_->OnScreenCapturePermissionUpdate(/*has_permission=*/true);

  EXPECT_FALSE(pane_view_->IsPermissionPaneVisible());
  EXPECT_TRUE(pane_view_->IsContentPaneVisible());
}
#endif
