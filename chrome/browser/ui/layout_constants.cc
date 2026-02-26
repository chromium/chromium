// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
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
    case LayoutConstant::kAppMenuProfileRowAvatarIconSize:
      return 24;
    case LayoutConstant::kAppMenuMaximumCharacterLength:
      return 30;
    case LayoutConstant::kBookmarkBarHeight: {
      // The fixed margin ensures the bookmark buttons appear centered relative
      // to the white space above and below.
      const int bookmark_bar_attached_vertical_margin = 6;
      return GetLayoutConstant(LayoutConstant::kBookmarkBarButtonHeight) +
             bookmark_bar_attached_vertical_margin;
    }
    case LayoutConstant::kBookmarkBarButtonHeight:
      return touch_ui ? 36 : 28;
    case LayoutConstant::kBookmarkBarButtonPadding:
      return GetLayoutConstant(LayoutConstant::kToolbarElementPadding);
    case LayoutConstant::kBookmarkBarButtonImageLabelPadding:
      return 6;
    case LayoutConstant::kWebAppMenuButtonSize:
      return 24;
    case LayoutConstant::kWebAppPageActionIconSize:
      // We must limit the size of icons in the title bar to avoid vertically
      // stretching the container view.
      return 16;
    case LayoutConstant::kLocationBarBubbleFontVerticalPadding:
      return 2;
    case LayoutConstant::kLocationBarBubbleAnchorVerticalInset:
      return 1;
    case LayoutConstant::kLocationBarChildInteriorPadding:
      return 3;
    case LayoutConstant::kLocationBarChildCornerRadius:
      return 12;
    case LayoutConstant::kLocationBarChipIconSize:
      return 16;
    case LayoutConstant::kLocationBarChipPadding:
      return 4;
    case LayoutConstant::kLocationBarElementPadding:
      return touch_ui ? 3 : 2;
    case LayoutConstant::kLocationBarPageInfoIconVerticalPadding:
      return touch_ui ? 3 : 5;
    case LayoutConstant::kLocationBarTrailingDecorationEdgePadding:
      return touch_ui ? 3 : 12;
    case LayoutConstant::kLocationBarTrailingDecorationInnerPadding:
      return touch_ui ? 3 : 8;
    case LayoutConstant::kLocationBarHeight:
      return touch_ui ? 36 : 34;
    case LayoutConstant::kLocationBarIconSize:
      return touch_ui ? 20 : 16;
    case LayoutConstant::kLocationBarLeadingIconSize:
      return GetLayoutConstant(LayoutConstant::kLocationBarIconSize);
    case LayoutConstant::kLocationBarTrailingIconSize:
      return 20;
    case LayoutConstant::kNewTabButtonLeadingMargin:
      return 0;
    case LayoutConstant::kStarRatingIconSize:
      return 14;
    case LayoutConstant::kTabAfterTitlePadding:
      return touch_ui ? 8 : 4;
    case LayoutConstant::kTabAlertIndicatorCaptureIconWidth:
      return 16;
    case LayoutConstant::kTabAlertIndicatorIconWidth:
      return touch_ui ? 12 : 16;
    case LayoutConstant::kTabCloseButtonSize:
      return touch_ui ? 24 : 16;
    case LayoutConstant::kTabHeight:
      return 34 + GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
    case LayoutConstant::kTabStripHeight:
      return GetLayoutConstant(LayoutConstant::kTabHeight) +
             GetLayoutConstant(LayoutConstant::kTabStripPadding);
    case LayoutConstant::kTabStripPadding:
      return 6;
    case LayoutConstant::kTabSeparatorHeight:
      return touch_ui ? 24 : 20;
    case LayoutConstant::kTabPreTitlePadding:
      return 8;
    case LayoutConstant::kTabStackDistance:
      return touch_ui ? 4 : 6;
    case LayoutConstant::kTabstripToolbarOverlap:
      return 1;
    case LayoutConstant::kToolbarButtonHeight:
      return touch_ui ? 48 : 34;
    case LayoutConstant::kToolbarButtonIconSize:
      return touch_ui ? kDefaultTouchableIconSize
                      : kDefaultIconSizeChromeRefresh;
    case LayoutConstant::kToolbarDividerCornerRadius:
      return 1;
    case LayoutConstant::kToolbarDividerHeight:
      return touch_ui ? 20 : 16;
    case LayoutConstant::kToolbarDividerSpacing:
      return 9;
    case LayoutConstant::kToolbarDividerWidth:
      return 2;
    case LayoutConstant::kToolbarElementPadding:
      return touch_ui ? 0 : 4;
    case LayoutConstant::kToolbarIconDefaultMargin:
      return touch_ui ? 0 : 2;
    case LayoutConstant::kLocationBarMargin:
      return touch_ui ? 12 : 9;
    case LayoutConstant::kToolbarHeightSidePanelInset:
      return 8;
    case LayoutConstant::kPageInfoIconSize:
      return 20;
    case LayoutConstant::kDownloadIconSize:
      return 20;
    case LayoutConstant::kMainBackgroundRegionCornerRadius:
    case LayoutConstant::kToolbarCornerRadius:
    case LayoutConstant::kVerticalTabCornerRadius:
      return 8;
    case LayoutConstant::kVerticalTabHeight:
      return 30;
    case LayoutConstant::kVerticalTabPinnedHeight:
      return 32;
    case LayoutConstant::kVerticalTabMinWidth:
      return 32;
    case LayoutConstant::kVerticalTabStripUncollapsedPadding:
      return 12;
    case LayoutConstant::kVerticalTabStripCollapsedPadding:
      return 8;
    case LayoutConstant::kVerticalTabStripCollapsedSeparatorWidth:
      return 24;
    case LayoutConstant::kVerticalTabStripTopButtonIconSize:
      return 20;
    case LayoutConstant::kVerticalTabStripBottomButtonIconSize:
      return 18;
    case LayoutConstant::kVerticalTabStripTopButtonPadding:
      return 4;
    case LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding:
      return 2;
    case LayoutConstant::kVerticalTabStripTopButtonContainerHeight:
      return 28;
    case LayoutConstant::kVerticalTabStripNewTabButtonSize:
      return 32;
    case LayoutConstant::kVerticalTabStripTopContainerButtonSize:
      return 28;
    case LayoutConstant::kVerticalTabPinnedBorderThickness:
      return 1;
    default:
      break;
  }
  NOTREACHED();
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (inset) {
    case DOWNLOAD_ICON:
      return gfx::Insets(4);

    case DOWNLOAD_ROW:
      return gfx::Insets::VH(8, 20);

    case LOCATION_BAR_ICON_INTERIOR_PADDING:
      return gfx::Insets::VH(2, 2);

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
      return gfx::Insets(touch_ui ? 12 : 7);

    case BROWSER_APP_MENU_CHIP_PADDING:
      if (touch_ui) {
        return GetLayoutInsets(TOOLBAR_BUTTON);
      } else {
        return gfx::Insets::TLBR(7, 4, 7, 6);
      }

    case WEB_APP_APP_MENU_CHIP_PADDING:
      return gfx::Insets::TLBR(0, 4, 0, 6);

    case AVATAR_CHIP_PADDING:
      if (touch_ui) {
        return GetLayoutInsets(TOOLBAR_BUTTON);
      } else {
        return gfx::Insets::TLBR(7, 10, 7, 4);
      }

    case TOOLBAR_INTERIOR_MARGIN:
      return touch_ui ? gfx::Insets::VH(4, 0) : gfx::Insets::VH(6, 5);

    case WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN:
      return gfx::Insets::VH(4, 0);

    case VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED:
      return gfx::Insets::VH(5, 14);

    case VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED:
      return gfx::Insets::VH(5, 6);
  }
  NOTREACHED();
}
