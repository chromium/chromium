// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/default_style.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
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
    case CONTEXT_OMNIBOX_PRIMARY:
    case CONTEXT_OMNIBOX_POPUP:
    case CONTEXT_OMNIBOX_SECTION_HEADER:
    case CONTEXT_OMNIBOX_DEEMPHASIZED: {
      const bool is_touch_ui = ui::TouchUiController::Get()->touch_ui();
      const bool use_gm3_text_style =
          OmniboxFieldTrial::IsGM3TextStyleEnabled();

      int desired_font_size = is_touch_ui ? 15 : 14;
      if (use_gm3_text_style) {
        desired_font_size = is_touch_ui
                                ? OmniboxFieldTrial::kFontSizeTouchUI.Get()
                                : OmniboxFieldTrial::kFontSizeNonTouchUI.Get();
      }

      const int omnibox_primary_delta =
          GetFontSizeDeltaBoundedByAvailableHeight(
              LocationBarView::GetAvailableTextHeight(), desired_font_size);
      details.size_delta = omnibox_primary_delta;
      if (context == CONTEXT_OMNIBOX_DEEMPHASIZED && !use_gm3_text_style) {
        --details.size_delta;
      }

      if (use_gm3_text_style) {
        if (context == CONTEXT_OMNIBOX_SECTION_HEADER) {
          --details.size_delta;
        }

        if (context == CONTEXT_OMNIBOX_PRIMARY ||
            context == CONTEXT_OMNIBOX_SECTION_HEADER) {
          details.weight = gfx::Font::Weight::MEDIUM;
        } else if (context == CONTEXT_OMNIBOX_POPUP ||
                   context == CONTEXT_OMNIBOX_DEEMPHASIZED) {
          details.weight = gfx::Font::Weight::NORMAL;
        }
      }
      break;
    }
#if BUILDFLAG(IS_WIN)
    case CONTEXT_WINDOWS10_NATIVE:
      // Adjusts default font size up to match Win10 modern UI.
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(15);
      break;
#endif
    case CONTEXT_IPH_BUBBLE_TITLE:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(18);
      if (features::IsChromeRefresh2023()) {
        details.weight = gfx::Font::Weight::MEDIUM;
      }
      break;
    case CONTEXT_IPH_BUBBLE_BODY:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(14);
      break;
    case CONTEXT_SIDE_PANEL_TITLE:
      details.size_delta =
          gfx::PlatformFont::GetFontSizeDeltaIgnoringUserOrLocaleSettings(13);
      break;
  }
}
