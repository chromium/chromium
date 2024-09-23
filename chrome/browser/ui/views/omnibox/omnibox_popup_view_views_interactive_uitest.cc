// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views_test.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget_utils.h"

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

  // Blurring the Omnibox w/ in-progress input (e.g. "foo") should result in
  // the on-focus colors.
  EXPECT_EQ(color_after_focus, location_bar()->background()->get_color());
  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       ClosePopupOnInactiveAreaClick) {
  if (!base::FeatureList::IsEnabled(
          features::kCloseOmniboxPopupOnInactiveAreaClick)) {
    return;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ui::test::EventGenerator event_generator(
      views::GetRootWindow(browser_view->GetWidget()),
      browser_view->GetNativeWindow());
  CreatePopupForTestQuery();
  event_generator.MoveMouseTo(
      browser_view->tabstrip()->tab_at(0)->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(omnibox_view()->HasFocus());
  EXPECT_FALSE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
}
