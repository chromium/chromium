// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

int GetLayoutConstant(LayoutConstant constant) {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (constant) {
    case APP_MENU_PROFILE_ROW_AVATAR_ICON_SIZE:
      return 24;
    case APP_MENU_MAXIMUM_CHARACTER_LENGTH:
      return 30;
    case BOOKMARK_BAR_HEIGHT: {
      // The fixed margin ensures the bookmark buttons appear centered relative
      // to the white space above and below.
      const int bookmark_bar_attached_vertical_margin =
          features::IsChromeRefresh2023() ? 6 : 4;
      return GetLayoutConstant(BOOKMARK_BAR_BUTTON_HEIGHT) +
             bookmark_bar_attached_vertical_margin;
    }
    case BOOKMARK_BAR_BUTTON_HEIGHT:
      return touch_ui ? 36 : 28;
    case BOOKMARK_BAR_BUTTON_PADDING:
      return features::IsChromeRefresh2023()
                 ? (touch_ui ? 0 : 8)
                 : GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
    case BOOKMARK_BAR_BUTTON_IMAGE_LABEL_PADDING:
      return features::IsChromeRefresh2023() ? 6 : 8;
    case WEB_APP_MENU_BUTTON_SIZE:
      return 24;
    case WEB_APP_PAGE_ACTION_ICON_SIZE:
      // We must limit the size of icons in the title bar to avoid vertically
      // stretching the container view.
      return 16;
    case LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING:
      return 2;
    case LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET:
      return 1;
    case LOCATION_BAR_CHILD_INTERIOR_PADDING:
      return 3;
    case LOCATION_BAR_CHILD_CORNER_RADIUS:
      return 12;
    case LOCATION_BAR_ELEMENT_PADDING:
      return touch_ui ? 3 : 2;
    case LOCATION_BAR_PAGE_INFO_ICON_VERTICAL_PADDING:
      return touch_ui ? 3 : 5;
    case LOCATION_BAR_LEADING_DECORATION_EDGE_PADDING:
      // TODO(manukh): See comment in `LocationBarView::Layout()`. We have too
      //   many feature permutations that would affect this and other layout
      //   constants. So instead of spreading the permutation logic here and
      //   elsewhere, its consolidated in `Layout()` and will be moved back here
      //   once we decide on a permutation.
      NOTREACHED_NORETURN();
    case LOCATION_BAR_TRAILING_DECORATION_EDGE_PADDING:
      return touch_ui ? 3 : 12;
    case LOCATION_BAR_HEIGHT:
      if (base::FeatureList::IsEnabled(omnibox::kOmniboxSteadyStateHeight) ||
          features::GetChromeRefresh2023Level() ==
              features::ChromeRefresh2023Level::kLevel2) {
        return touch_ui ? 36 : 34;
      } else {
        return touch_ui ? 36 : 28;
      }
    case LOCATION_BAR_ICON_SIZE:
      return touch_ui ? 20 : 16;
    case LOCATION_BAR_CHIP_ICON_SIZE:
      return 16;
    case LOCATION_BAR_LEADING_ICON_SIZE:
      return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    case LOCATION_BAR_TRAILING_ICON_SIZE:
      return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
                 ? 20
                 : GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    case TAB_AFTER_TITLE_PADDING:
      return touch_ui ? 8 : 4;
    case TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH:
      return 16;
    case TAB_ALERT_INDICATOR_ICON_WIDTH:
      return touch_ui ? 12 : 16;
    case TAB_CLOSE_BUTTON_SIZE:
      return touch_ui ? 24 : 16;
    case TAB_HEIGHT: {
      bool use_touch_padding = touch_ui && !features::IsChromeRefresh2023();
#if BUILDFLAG(IS_CHROMEOS)
      use_touch_padding &= !chromeos::features::IsRoundedWindowsEnabled();
#endif
      return (use_touch_padding ? 41 : 34) +
             GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
    }
    case TAB_STRIP_HEIGHT:
      return GetLayoutConstant(TAB_HEIGHT) +
             GetLayoutConstant(TAB_STRIP_PADDING);
    case TAB_STRIP_PADDING:
      return features::IsChromeRefresh2023() ? 6 : 0;
    case TAB_SEPARATOR_HEIGHT:
      // TODO (crbug.com/1451400): ChromeRefresh2023 needs different values for
      // this constant.
      return touch_ui ? 24 : 20;
    case TAB_PRE_TITLE_PADDING:
      return 8;
    case TAB_STACK_DISTANCE:
      return touch_ui ? 4 : 6;
    case TABSTRIP_REGION_VIEW_CONTROL_PADDING:
      // TODO (crbug.com/1451400): ChromeRefresh2023 needs different values for
      // this constant.
      return 8;
    case TABSTRIP_TOOLBAR_OVERLAP:
      // Because tab scrolling puts the tabstrip on a separate layer,
      // changing paint order, this overlap isn't compatible with scrolling.
      if (base::FeatureList::IsEnabled(features::kScrollableTabStrip))
        return 0;
      return 1;
    case TOOLBAR_BUTTON_HEIGHT:
      if (features::IsChromeRefresh2023()) {
        return touch_ui ? 48 : 34;
      } else {
        return touch_ui ? 48 : 28;
      }
    case TOOLBAR_DIVIDER_CORNER_RADIUS:
      return 1;
    case TOOLBAR_DIVIDER_HEIGHT:
      return touch_ui ? 20 : 16;
    case TOOLBAR_DIVIDER_SPACING:
      return 9;
    case TOOLBAR_DIVIDER_WIDTH:
      return 2;
    case TOOLBAR_ELEMENT_PADDING:
      return touch_ui ? 0 : 4;
    case TOOLBAR_ICON_DEFAULT_MARGIN:
      if (features::IsChromeRefresh2023()) {
        return touch_ui ? 0 : 2;
      } else {
        return GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
      }
    case TOOLBAR_STANDARD_SPACING:
      if (features::IsChromeRefresh2023()) {
        return touch_ui ? 12 : 9;
      } else {
        return touch_ui ? 12 : 8;
      }
    case PAGE_INFO_ICON_SIZE:
      return features::IsChromeRefresh2023() ? 20 : 16;
    case DOWNLOAD_ICON_SIZE:
      return features::IsChromeRefresh2023() ? 20 : 16;
    case TOOLBAR_CORNER_RADIUS:
      return 8;
    default:
      break;
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (inset) {
    case DOWNLOAD_ICON:
      return gfx::Insets(4);

    case DOWNLOAD_ROW:
      return gfx::Insets::VH(8, features::IsChromeRefresh2023() ? 20 : 16);
    case LOCATION_BAR_ICON_INTERIOR_PADDING:
      if (features::IsChromeRefresh2023()) {
        return gfx::Insets::VH(2, 2);
      }
      return touch_ui ? gfx::Insets::VH(5, 10) : gfx::Insets::VH(4, 8);

    case LOCATION_BAR_PAGE_INFO_ICON_PADDING:
      return touch_ui ? gfx::Insets::VH(5, 10) : gfx::Insets::VH(4, 4);

    case LOCATION_BAR_PAGE_ACTION_ICON_PADDING:
      return touch_ui ? gfx::Insets::VH(5, 10) : gfx::Insets::VH(2, 2);

    case TOOLBAR_ACTION_VIEW: {
      // TODO(afakhry): Unify all toolbar button sizes on all platforms.
      // https://crbug.com/822967.
      return gfx::Insets(touch_ui ? 10 : 0);
    }

    case TOOLBAR_BUTTON:
      return gfx::Insets(touch_ui ? 12
                                  : (features::IsChromeRefresh2023() ? 7 : 6));

    case BROWSER_APP_MENU_CHIP_PADDING:
      if (touch_ui || !features::IsChromeRefresh2023()) {
        return GetLayoutInsets(TOOLBAR_BUTTON);
      } else {
        return gfx::Insets::TLBR(7, 4, 7, 6);
      }

    case AVATAR_CHIP_PADDING:
      if (touch_ui || !features::IsChromeRefresh2023()) {
        return GetLayoutInsets(TOOLBAR_BUTTON);
      } else {
        return gfx::Insets::TLBR(7, 10, 7, 4);
      }

    case TOOLBAR_INTERIOR_MARGIN:
      if (features::IsChromeRefresh2023()) {
        return touch_ui ? gfx::Insets() : gfx::Insets::VH(6, 5);
      } else {
        return touch_ui ? gfx::Insets() : gfx::Insets::VH(4, 8);
      }

    case WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN:
      return gfx::Insets::VH(4, 0);
  }
  NOTREACHED();
  return gfx::Insets();
}
