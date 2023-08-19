// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

// Check that the location bar background (and the background of the textfield
// it contains) changes when it receives focus, and matches the popup background
// color.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       PopupMatchesLocationBarBackground) {
  // In dark mode the omnibox focused and unfocused colors are the same, which
  // makes this test fail; see comments below.
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetNativeTheme()
      ->set_use_dark_colors(false);

  // Start with the Omnibox unfocused.
  omnibox_view()->GetFocusManager()->ClearFocus();
  const SkColor color_before_focus = location_bar()->background()->get_color();
  EXPECT_EQ(color_before_focus, omnibox_view()->GetBackgroundColor());

  // Give the Omnibox focus and get its focused color.
  omnibox_view()->RequestFocus();
  const SkColor color_after_focus = location_bar()->background()->get_color();

  // Sanity check that the colors are different, otherwise this test will not be
  // testing anything useful. It is possible that a particular theme could
  // configure these colors to be the same. In that case, this test should be
  // updated to detect that, or switch to a theme where they are different.
  EXPECT_NE(color_before_focus, color_after_focus);
  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());

  // The background is hosted in the view that contains the results area.
  CreatePopupForTestQuery();
  views::View* background_host = popup_view()->parent();
  EXPECT_EQ(color_after_focus, background_host->background()->get_color());

  omnibox_view()->GetFocusManager()->ClearFocus();

  if (features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(
          omnibox::kOmniboxSteadyStateBackgroundColor)) {
    // With CR23, blurring the Omnibox w/ in-progress input (e.g. "foo") should
    // result in the on-focus colors.
    EXPECT_EQ(color_after_focus, location_bar()->background()->get_color());
    EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());
  } else {
    // Without CR23, blurring the Omnibox w/ in-progress input (e.g. "foo")
    // should restore the original colors.
    EXPECT_EQ(color_before_focus, location_bar()->background()->get_color());
    EXPECT_EQ(color_before_focus, omnibox_view()->GetBackgroundColor());
  }
}
