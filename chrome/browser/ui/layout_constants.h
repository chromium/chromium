// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum LayoutConstant {
  // The size of the avatar icon in the profile row of the app menu.
  APP_MENU_PROFILE_ROW_AVATAR_ICON_SIZE,

  // The maximum character length for strings in the app menu.
  APP_MENU_MAXIMUM_CHARACTER_LENGTH,

  // The height of Bookmarks Bar when attached to the toolbar. The height of the
  // Bookmarks Bar is larger than the BOOKMARK_BAR_BUTTON_HEIGHT by a fixed
  // amount.
  BOOKMARK_BAR_HEIGHT,

  // The height of a button within the Bookmarks Bar.
  BOOKMARK_BAR_BUTTON_HEIGHT,

  // The horizontal padding between buttons within the Bookmarks Bar.
  BOOKMARK_BAR_BUTTON_PADDING,

  // The horizontal padding between the image and the title of the bookmark
  // button.
  BOOKMARK_BAR_BUTTON_IMAGE_LABEL_PADDING,

  // The size of icons used in Download bubbles.
  // TODO(crbug.com/40214740): We should be sourcing the size of the file icon
  // from
  // the layout
  // provider rather than relying on hardcoded constants.
  DOWNLOAD_ICON_SIZE,

  // The vertical padding between the edge of a location bar bubble and its
  // contained text.
  LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING,

  // The vertical inset to apply to the bounds of a location bar bubble's anchor
  // view, to bring the bubble closer to the anchor.  This compensates for the
  // space between the bottoms of most such views and the visible bottoms of the
  // images inside.
  LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET,

  // The internal padding to use inside children of the location bar.
  LOCATION_BAR_CHILD_INTERIOR_PADDING,

  // The corner radius to use for children of the location bar.
  LOCATION_BAR_CHILD_CORNER_RADIUS,

  // The size of icons within chips inside the location bar.
  LOCATION_BAR_CHIP_ICON_SIZE,

  // The internal padding to use inside an indicator chip, permission request
  // chip and between chips in the location bar.
  LOCATION_BAR_CHIP_PADDING,

  // The vertical and horizontal padding inside the border.
  LOCATION_BAR_ELEMENT_PADDING,

  // The height to be occupied by the LocationBar.
  LOCATION_BAR_HEIGHT,

  // The vertical margins from the page info icon
  LOCATION_BAR_PAGE_INFO_ICON_VERTICAL_PADDING,

  // The leading edge space in the omnibox from the LHS icons used in Chrome
  // with the chrome refresh flag.
  LOCATION_BAR_LEADING_DECORATION_EDGE_PADDING,

  // The trailing edge space in the omnibox from the RHS icons used in Chrome
  // with the chrome refresh flag.
  LOCATION_BAR_TRAILING_DECORATION_EDGE_PADDING,

  // The padding between trailing edge decorations and the inner views of the
  // omnibox.
  LOCATION_BAR_TRAILING_DECORATION_INNER_PADDING,

  // The size of the icons used inside the LocationBar.
  // TODO(crbug.com/40883435): Deprecate this after the size of all location bar
  // icons have moved to
  // either `LOCATION_BAR_LEADING_ICON_SIZE` or
  // `LOCATION_BAR_TRAILING_ICON_SIZE`
  LOCATION_BAR_ICON_SIZE,

  // The size of the leading icons used inside the LocationBar.
  LOCATION_BAR_LEADING_ICON_SIZE,

  // The size of the trailing icons used inside the LocationBar.
  LOCATION_BAR_TRAILING_ICON_SIZE,

  // The size of icons used in PageInfo bubbles.
  PAGE_INFO_ICON_SIZE,

  // Padding after the tab title.
  TAB_AFTER_TITLE_PADDING,

  // Width of the alert indicator shown for a tab using media capture.
  TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH,

  // Width of the alert indicator icon displayed in the tab. The same width is
  // used for all 3 states of normal, hovered and pressed.
  TAB_ALERT_INDICATOR_ICON_WIDTH,

  // Width and height of the tab close button.
  TAB_CLOSE_BUTTON_SIZE,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  TAB_HEIGHT,

  // The total tab strip height, including all interior padding.
  TAB_STRIP_HEIGHT,

  // The padding value shared between the area above the tab, the bottom of the
  // detached tab, and on all sides of the controls padding.
  TAB_STRIP_PADDING,

  // The height of a separator in the tabstrip.
  TAB_SEPARATOR_HEIGHT,

  // Padding before the tab title.
  TAB_PRE_TITLE_PADDING,

  // The distance between the edge of one tab to the corresponding edge or the
  // subsequent tab when tabs are stacked.
  TAB_STACK_DISTANCE,

  // In refresh, tabs are drawn with an extension into the toolbar's
  // space to prevent a gap from appearing between the toolbar and the
  // bottom of tabs on some non-integral scales.
  // TODO(tbergquist): Remove this after pixel canvas or any deeper fix to
  // non-pixel-aligned drawing goes in.  See https://crbug.com/765723.
  TABSTRIP_TOOLBAR_OVERLAP,

  // The total height, including icons and insets, of buttons in the toolbar.
  TOOLBAR_BUTTON_HEIGHT,

  // The corner radius for a divider in the toolbar.
  TOOLBAR_DIVIDER_CORNER_RADIUS,

  // The height for a divider in the toolbar.
  TOOLBAR_DIVIDER_HEIGHT,

  // The horizontal space on either side of a divider in the toolbar.
  TOOLBAR_DIVIDER_SPACING,

  // The width for a divider in the toolbar.
  TOOLBAR_DIVIDER_WIDTH,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_ELEMENT_PADDING,

  // Default margin of the toolbar icons set by the layout manager.
  TOOLBAR_ICON_DEFAULT_MARGIN,

  // The horizontal space between most items in the toolbar.
  TOOLBAR_STANDARD_SPACING,

  // corner radius on the top of the toolbar introduced in chrome refresh 2023
  TOOLBAR_CORNER_RADIUS,

  // The size of the app menu button in a web app browser window.
  WEB_APP_MENU_BUTTON_SIZE,

  // The size of page action icons in a web app title bar.
  WEB_APP_PAGE_ACTION_ICON_SIZE,
};

enum LayoutInset {
  // The padding around icons used in Download bubbles.
  DOWNLOAD_ICON,

  // The padding around rows used in Download bubbles.
  DOWNLOAD_ROW,

  // The padding used around the icon inside the LocationBar. The full width of
  // the icon would be LOCATION_BAR_ICON_SIZE + 2 * inset.width(). The full
  // height of the icon would be LOCATION_BAR_ICON_SIZE + 2 * inset.height().
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

  // The padding around the profile menu chip.
  AVATAR_CHIP_PADDING,

  // The padding between the edges of the toolbar and its content.
  TOOLBAR_INTERIOR_MARGIN,

  // The padding between the edges of the toolbar and its content when the webui
  // tab strip is enabled. Special handling is needed as when the browser is
  // maximized and the tabstrip is collapsed the toolbar will sit flush with the
  // edge of the screen.
  WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN,
};

int GetLayoutConstant(LayoutConstant constant);

gfx::Insets GetLayoutInsets(LayoutInset inset);

#endif  // CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
