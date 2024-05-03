// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui_test.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

// ChromeOS environment doesn't instantiate the NewWebUI<OmniboxPopupUI>
// in the factory's GetWebUIFactoryFunction, so these don't work there yet.
// Also avoid burdening test bots on mobile platforms where webui omnibox
// isn't ready and the platform-specific views implementation is in scope.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Check that the location bar background (and the background of the textfield
// it contains) changes when it receives focus, and matches the popup background
// color.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest,
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
  views::View* background_host = location_bar();
  EXPECT_EQ(color_after_focus, background_host->background()->get_color());

  omnibox_view()->GetFocusManager()->ClearFocus();

  // Blurring the Omnibox w/ in-progress input (e.g. "foo") should result in
  // the on-focus colors.
  EXPECT_EQ(color_after_focus, location_bar()->background()->get_color());
  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewWebUITest, PopupLoadsAndAcceptsCalls) {
  WaitForHandler();
  popup_view()->presenter_->Show();
  popup_view()->UpdatePopupAppearance();
  OmniboxPopupSelection selection(OmniboxPopupSelection::kNoMatch);
  popup_view()->OnSelectionChanged(selection, selection);
  popup_view()->ProvideButtonFocusHint(0);
  popup_view()->presenter_->Hide();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
