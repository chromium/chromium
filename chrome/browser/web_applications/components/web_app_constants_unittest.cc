// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(WebAppConstants, IsSuccess) {
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessNewInstall));
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessAlreadyInstalled));

  EXPECT_FALSE(IsSuccess(InstallResultCode::kFailedUnknownReason));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kExpectedAppIdCheckFailed));
}

TEST(WebAppConstants, ResolveEffectiveDisplayMode) {
  // When user_display_mode indicates a user preference for opening in
  // a browser tab, we open in a browser tab.
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(DisplayMode::kBrowser,
                                        DisplayMode::kBrowser));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(DisplayMode::kMinimalUi,
                                        DisplayMode::kBrowser));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(DisplayMode::kStandalone,
                                        DisplayMode::kBrowser));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(DisplayMode::kFullscreen,
                                        DisplayMode::kBrowser));

  // When user_display_mode indicates a user preference for opening in
  // a standalone window, we open in a minimal-ui window (for app_display_mode
  // 'browser' or 'minimal-ui') or a standalone window (for app_display_mode
  // 'standalone' or 'fullscreen').
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(DisplayMode::kBrowser,
                                        DisplayMode::kStandalone));
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(DisplayMode::kMinimalUi,
                                        DisplayMode::kStandalone));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(DisplayMode::kStandalone,
                                        DisplayMode::kStandalone));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(DisplayMode::kFullscreen,
                                        DisplayMode::kStandalone));
}

}  // namespace web_app
