// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum class LayoutConstant {
  // The size of the avatar icon in the profile row of the app menu.
  kAppMenuProfileRowAvatarIconSize,

  // The maximum character length for strings in the app menu.
  kAppMenuMaximumCharacterLength,

  // The height of Bookmarks Bar when attached to the toolbar. The height of the
  // Bookmarks Bar is larger than the kBookmarkBarHeight by a fixed
  // amount.
  kBookmarkBarHeight,

  // The height of a button within the Bookmarks Bar.
  kBookmarkBarButtonHeight,

  // The horizontal padding between buttons within the Bookmarks Bar.
  kBookmarkBarButtonPadding,

  // The horizontal padding between the image and the title of the bookmark
  // button.
  kBookmarkBarButtonImageLabelPadding,

  // The size of icons used in Download bubbles.
  // TODO(crbug.com/40214740): We should be sourcing the size of the file icon
  // from the layout provider rather than relying on hardcoded constants.
  kDownloadIconSize,

  // The vertical padding between the edge of a location bar bubble and its
  // contained text.
  kLocationBarBubbleFontVerticalPadding,

  // The vertical inset to apply to the bounds of a location bar bubble's anchor
  // view, to bring the bubble closer to the anchor.  This compensates for the
  // space between the bottoms of most such views and the visible bottoms of the
  // images inside.
  kLocationBarBubbleAnchorVerticalInset,

  // The internal padding to use inside children of the location bar.
  kLocationBarChildInteriorPadding,

  // The corner radius to use for children of the location bar.
  kLocationBarChildCornerRadius,

  // The size of icons within chips inside the location bar.
  kLocationBarChipIconSize,

  // The internal padding to use inside an indicator chip, permission request
  // chip and between chips in the location bar.
  kLocationBarChipPadding,

  // The vertical and horizontal padding inside the border.
  kLocationBarElementPadding,

  // The height to be occupied by the LocationBar.
  kLocationBarHeight,

  // The vertical margins from the page info icon
  kLocationBarPageInfoIconVerticalPadding,

  // The leading edge space in the omnibox from the LHS icons used in Chrome
  // with the chrome refresh flag.
  // TODO(manukh): See comment in `LocationBarView::Layout()`. We have too
  // many feature permutations that would affect this and other layout
  // constants, so instead of spreading the permutation logic here and
  // elsewhere, it's consolidated in `Layout()` and will be moved back
  // here once we decide on a permutation.
  // kLocationBarLeadingDecorationEdgePadding,

  // The trailing edge space in the omnibox from the RHS icons used in Chrome
  // with the chrome refresh flag.
  kLocationBarTrailingDecorationEdgePadding,

  // The padding between trailing edge decorations and the inner views of the
  // omnibox.
  kLocationBarTrailingDecorationInnerPadding,

  // The size of the icons used inside the LocationBar.
  // TODO(crbug.com/40883435): Deprecate this after the size of all location bar
  // icons have moved to either `kLocationBarLeadingIconSize` or
  // `kLocationBarTrailingIconSize`
  kLocationBarIconSize,

  // The size of the leading icons used inside the LocationBar.
  kLocationBarLeadingIconSize,

  // The size of the trailing icons used inside the LocationBar.
  kLocationBarTrailingIconSize,

  // The horizontal margin between location bar and other toolbar items.
  kLocationBarMargin,

  // The corner radius of the MainBackgroundRegion when tool bar height side
  // panel is visible
  kMainBackgroundRegionCornerRadius,

  // Additional space beyond kTabStripPadding between the tab strip and the
  // new tab button.
  kNewTabButtonLeadingMargin,

  // The size of icons used in PageInfo bubbles.
  kPageInfoIconSize,

  // The size of icons in star rating view.
  kStarRatingIconSize,

  // Padding after the tab title.
  kTabAfterTitlePadding,

  // Width of the alert indicator shown for a tab using media capture.
  kTabAlertIndicatorCaptureIconWidth,

  // Width of the alert indicator icon displayed in the tab. The same width is
  // used for all 3 states of normal, hovered and pressed.
  kTabAlertIndicatorIconWidth,

  // Width and height of the tab close button.
  kTabCloseButtonSize,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  kTabHeight,

  // The total tab strip height, including all interior padding.
  kTabStripHeight,

  // The padding value shared between the area above the tab, the bottom of the
  // detached tab, and on all sides of the controls padding.
  kTabStripPadding,

  // The height of a separator in the tabstrip.
  kTabSeparatorHeight,

  // Padding before the tab title.
  kTabPreTitlePadding,

  // The distance between the edge of one tab to the corresponding edge or the
  // subsequent tab when tabs are stacked.
  kTabStackDistance,

  // In refresh, tabs are drawn with an extension into the toolbar's
  // space to prevent a gap from appearing between the toolbar and the
  // bottom of tabs on some non-integral scales.
  // TODO(tbergquist): Remove this after pixel canvas or any deeper fix to
  // non-pixel-aligned drawing goes in.  See https://crbug.com/765723.
  kTabstripToolbarOverlap,

  // The total height, including icons and insets, of buttons in the toolbar.
  kToolbarButtonHeight,

  // The icon size for toolbar buttons.
  kToolbarButtonIconSize,

  // The corner radius for a divider in the toolbar.
  kToolbarDividerCornerRadius,

  // The height for a divider in the toolbar.
  kToolbarDividerHeight,

  // The horizontal space on either side of a divider in the toolbar.
  kToolbarDividerSpacing,

  // The width for a divider in the toolbar.
  kToolbarDividerWidth,

  // Additional horizontal padding between the elements in the toolbar.
  kToolbarElementPadding,

  // Default margin of the toolbar icons set by the layout manager.
  kToolbarIconDefaultMargin,

  // corner radius on the top of the toolbar introduced in chrome refresh 2023
  kToolbarCornerRadius,

  // The padding between the bottom of the tab strip and top of the toolbar
  // height side panel.
  kToolbarHeightSidePanelInset,

  // The corner radius used for borders, fill, and hover targets with vertical
  // tabs.
  kVerticalTabCornerRadius,

  // The height of an unpinned vertical tab.
  kVerticalTabHeight,

  // The height of a pinned vertical tab.
  kVerticalTabPinnedHeight,

  // The minimum possible width for a vertical tab.
  kVerticalTabMinWidth,

  // The width of the border stroke around pinned tabs in a vertical tab strip.
  kVerticalTabPinnedBorderThickness,

  // The padding between the sides/bottom of the vertical tab strip and its
  // content when in the uncollapsed state.
  kVerticalTabStripUncollapsedPadding,

  // The padding between the sides/bottom of the vertical tab strip and its
  // content when in the collapsed state.
  kVerticalTabStripCollapsedPadding,

  // The width of the separator in the vertical tab strip when collapsed.
  kVerticalTabStripCollapsedSeparatorWidth,

  // The icon size of top buttons in the vertical tab strip.
  kVerticalTabStripTopButtonIconSize,

  // The padding between the buttons in the top container of the vertical tab
  // strip. When it is collapsed, this is vertical padding. When it is
  // uncollapsed and expanded, this is horizontal padding.
  kVerticalTabStripTopButtonPadding,

  // The icon size of bottom buttons in the vertical tab strip.
  kVerticalTabStripBottomButtonIconSize,

  // The vertical or horizontal padding between two buttons (tab groups and tab
  // search) that have flat edges in the top container of the vertical tab
  // strip.
  kVerticalTabStripFlatEdgeButtonPadding,

  // The default height of the top container for the vertical tab strip when
  // uncollapsed.
  kVerticalTabStripTopButtonContainerHeight,

  // The default height and width of the new tab button for the vertical tab
  // strip.
  kVerticalTabStripNewTabButtonSize,

  // The default height and width of the tab groups and tab search buttons for
  // the vertical tab strip.
  kVerticalTabStripTopContainerButtonSize,

  // The size of the app menu button in a web app browser window.
  kWebAppMenuButtonSize,

  // The size of page action icons in a web app title bar.
  kWebAppPageActionIconSize,

  kLast = kWebAppPageActionIconSize
};

enum LayoutInset {
  // The padding around icons used in Download bubbles.
  DOWNLOAD_ICON,

  // The padding around rows used in Download bubbles.
  DOWNLOAD_ROW,

  // The padding used around the icon inside the LocationBar. The full width of
  // the icon would be kLocationBarIconSize + 2 * inset.width(). The full
  // height of the icon would be kLocationBarIconSize + 2 * inset.height().
  // Icons may additionally be spaced horizontally by
  // LOCATION_BAR_ELEMENT_PADDING, but this region is not part of the icon view
  // (e.g. does not highlight on hover).
  LOCATION_BAR_ICON_INTERIOR_PADDING,

  // The page info icon in the location bar has different insets than the other
  // icons with chrome refresh flag.
  LOCATION_BAR_PAGE_INFO_ICON_PADDING,

  // The page action icons in the location bar have different insets than the
  // other icons with chrome refresh flag.
  LOCATION_BAR_PAGE_ACTION_ICON_PADDING,

  // The padding inside the border of a toolbar action view button.
  TOOLBAR_ACTION_VIEW,

  // The padding inside the border of a toolbar button (around the image).
  TOOLBAR_BUTTON,

  // The padding around the browser app menu chip.
  BROWSER_APP_MENU_CHIP_PADDING,

  // The padding around the app menu chip in a web app browser window.
  WEB_APP_APP_MENU_CHIP_PADDING,

  // The padding around the profile menu chip.
  AVATAR_CHIP_PADDING,

  // The padding between the edges of the toolbar and its content.
  TOOLBAR_INTERIOR_MARGIN,

  // The padding between the edges of the toolbar and its content when the webui
  // tab strip is enabled. Special handling is needed as when the browser is
  // maximized and the tabstrip is collapsed the toolbar will sit flush with the
  // edge of the screen.
  WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN,

  // The insets for the buttons in the bottom container of the vertical tab
  // strip when it is uncollapsed.
  VERTICAL_TAB_STRIP_BOTTOM_BUTTON_UNCOLLAPSED,

  // The insets for the buttons in the bottom container of the vertical tab
  // strip when it is collapsed.
  VERTICAL_TAB_STRIP_BOTTOM_BUTTON_COLLAPSED,
};

// Layout constants for the split tabs button status indicator.
inline constexpr int kSplitTabsStatusIndicatorWidth = 14;
inline constexpr int kSplitTabsStatusIndicatorHeight = 2;
inline constexpr int kSplitTabsStatusIndicatorSpacing = 1;

// Default icon size for toolbar buttons.
inline constexpr int kDefaultIconSizeChromeRefresh = 20;
// Default icon size for toolbar buttons in touch mode.
inline constexpr int kDefaultTouchableIconSize = 24;

int GetLayoutConstant(LayoutConstant constant);

gfx::Insets GetLayoutInsets(LayoutInset inset);

#endif  // CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
