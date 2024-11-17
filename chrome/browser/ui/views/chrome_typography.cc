// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/default_style.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font.h"

int GetFontSizeDeltaBoundedByAvailableHeight(int available_height,
                                             int desired_font_size) {
  int size_delta =
      gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(
          desired_font_size);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = bundle.GetFontListWithDelta(size_delta);

  // Shrink large fonts to ensure they fit. Default fonts should fit already.
  // TODO(tapted): Move DeriveWithHeightUpperBound() to ui::ResourceBundle to
  // take advantage of the font cache.
  int user_or_locale_delta =
      size_delta + gfx::PlatformFont::kDefaultBaseFontSize - desired_font_size;
  base_font = base_font.DeriveWithHeightUpperBound(available_height);

  return base_font.GetFontSize() - gfx::PlatformFont::kDefaultBaseFontSize +
         user_or_locale_delta;
}

void ApplyCommonFontStyles(int context,
                           int style,
                           ui::ResourceBundle::FontDetails& details) {
  switch (context) {
    case CONTEXT_TOOLBAR_BUTTON: {
      int height = ui::TouchUiController::Get()->touch_ui() ? 22 : 17;
      // We only want the font size to be constrained by available height, and
      // don't actually have a target font size, so we just need to supply any
      // sufficiently-large value for the second argument here. |height| will
      // always be sufficiently large, since dips are smaller than pts.
      details.size_delta =
          GetFontSizeDeltaBoundedByAvailableHeight(height, height);
      break;
    }
    case CONTEXT_TAB_COUNTER: {
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(14);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    }
    case CONTEXT_DEEMPHASIZED:
    case CONTEXT_OMNIBOX_PRIMARY:
    case CONTEXT_OMNIBOX_POPUP:
    case CONTEXT_OMNIBOX_SECTION_HEADER:
    case CONTEXT_OMNIBOX_POPUP_ROW_CHIP: {
      const bool is_touch_ui = ui::TouchUiController::Get()->touch_ui();
      int desired_font_size = is_touch_ui ? 15 : 14;
      const int omnibox_primary_delta =
          GetFontSizeDeltaBoundedByAvailableHeight(
              LocationBarView::GetAvailableTextHeight(), desired_font_size);
      details.size_delta = omnibox_primary_delta;
      if (context == CONTEXT_DEEMPHASIZED) {
        --details.size_delta;
      } else if (context == CONTEXT_OMNIBOX_POPUP_ROW_CHIP) {
        details.size_delta -= 2;
      }
      break;
    }
    case CONTEXT_IPH_BUBBLE_TITLE:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(18);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case CONTEXT_IPH_BUBBLE_BODY:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(14);
      break;
    case CONTEXT_SIDE_PANEL_TITLE:
    case CONTEXT_TOAST_BODY_TEXT:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(13);
      break;
  }
}
