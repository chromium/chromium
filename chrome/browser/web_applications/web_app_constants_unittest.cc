// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include "chrome/browser/web_applications/user_display_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(WebAppConstants, ResolveEffectiveDisplayMode) {
  // When user_display_mode indicates a user preference for opening in
  // a browser tab, we open in a browser tab.
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));

  // When user_display_mode indicates a user preference for opening in
  // a standalone window, we open in a minimal-ui window (for app_display_mode
  // 'browser' or 'minimal-ui') or a standalone window (for app_display_mode
  // 'standalone' or 'fullscreen').
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppConstants,
     ResolveEffectiveDisplayModeWithDisplayOverridesPreferUserMode) {
  // When user_display_mode indicates a user preference for opening in
  // a browser tab, we open in a browser tab even if display_overrides
  // are specified
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kStandalone);

  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
}

TEST(WebAppConstants,
     ResolveEffectiveDisplayModeWithDisplayOverridesFallbackToDisplayMode) {
  // When user_display_mode indicates a user preference for opening in
  // a standalone window, and the only display modes provided for
  // display_overrides contain only 'fullscreen' or 'browser',  open in a
  // minimal-ui window (for app_display_mode 'browser' or 'minimal-ui') or a
  // standalone window (for app_display_mode 'standalone' or 'fullscreen').
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kFullscreen);

  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppConstants, ResolveEffectiveDisplayModeWithDisplayOverrides) {
  // When user_display_mode indicates a user preference for opening in
  // a standalone window, and return the first entry that is either
  // 'standalone' or 'minimal-ui' in display_override
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kFullscreen);
  app_display_mode_overrides.push_back(DisplayMode::kBrowser);
  app_display_mode_overrides.push_back(DisplayMode::kStandalone);

  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppConstants, ResolveEffectiveDisplayModeWithIsolatedApp) {
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                /*app_display_mode=*/DisplayMode::kBrowser,              //
                /*app_display_mode_overrides=*/{DisplayMode::kBrowser},  //
                /*user_display_mode=*/UserDisplayMode::kBrowser,         //
                /*is_isolated=*/true));

  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                /*app_display_mode=*/DisplayMode::kMinimalUi,     //
                /*app_display_mode_overrides=*/{},                //
                /*user_display_mode=*/UserDisplayMode::kBrowser,  //
                /*is_isolated=*/true));
}

}  // namespace web_app
