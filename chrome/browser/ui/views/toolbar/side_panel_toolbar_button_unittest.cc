// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"

#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"

class SidePanelToolbarButtonTest : public TestWithBrowserView {
 public:
  SidePanelToolbarButton* GetSidePanelToolbarButton() {
    return browser_view()->toolbar()->GetSidePanelButton();
  }
};

// Verify correct buttons are shown when side panel alignment is changed.
TEST_F(SidePanelToolbarButtonTest, SetCorrectIconInLTR) {
  SidePanelToolbarButton* const side_panel_button = GetSidePanelToolbarButton();
  ASSERT_TRUE(side_panel_button != nullptr);

  EXPECT_EQ(side_panel_button->context_menu_controller(), nullptr);

  // Set right aligned side panel.
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);

  // Mocked preference objects that listen to PrefChangeRegistrar will not work
  // as expected. So we need to simulate this by calling UpdateToolbarButtonIcon
  // directly.
  side_panel_button->UpdateToolbarButtonIcon();
  const ui::ColorProvider* color_provider =
      side_panel_button->GetColorProvider();

  // Right aligned side panels should use the right aligned icon.
  ASSERT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(side_panel_button->GetImage(views::Button::STATE_NORMAL)),
      gfx::Image(gfx::CreateVectorIcon(
          kSidePanelIcon, color_provider->GetColor(kColorToolbarButtonIcon)))));

  // Left aligned side panels should use the left aligned icon.
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  side_panel_button->UpdateToolbarButtonIcon();
  ASSERT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(side_panel_button->GetImage(views::Button::STATE_NORMAL)),
      gfx::Image(gfx::CreateVectorIcon(
          kSidePanelLeftIcon,
          color_provider->GetColor(kColorToolbarButtonIcon)))));
}

// Verify correct buttons are shown in RTL mode.
TEST_F(SidePanelToolbarButtonTest, SetCorrectIconInRTL) {
  // Enter RTL mode by using an RTL language.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  SidePanelToolbarButton* const side_panel_button = GetSidePanelToolbarButton();
  ASSERT_TRUE(side_panel_button != nullptr);

  EXPECT_EQ(side_panel_button->context_menu_controller(), nullptr);

  // Set right aligned side panel.
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, true);

  // Mocked preference objects that listen to PrefChangeRegistrar will not work
  // as expected. So we need to simulate this by calling UpdateToolbarButtonIcon
  // directly.
  side_panel_button->UpdateToolbarButtonIcon();
  const ui::ColorProvider* color_provider =
      side_panel_button->GetColorProvider();

  // Right aligned side panels should use the right aligned icon.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(side_panel_button->GetImage(views::Button::STATE_NORMAL)),
      gfx::Image(gfx::CreateVectorIcon(
          kSidePanelIcon, color_provider->GetColor(kColorToolbarButtonIcon)))));

  // Left aligned side panels should use the left aligned icon.
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);
  side_panel_button->UpdateToolbarButtonIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(side_panel_button->GetImage(views::Button::STATE_NORMAL)),
      gfx::Image(gfx::CreateVectorIcon(
          kSidePanelLeftIcon,
          color_provider->GetColor(kColorToolbarButtonIcon)))));
}
