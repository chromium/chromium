// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_

#include <memory>

#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_provider.h"

enum ChromeInsetsMetric {
  // Padding around buttons on the bookmarks bar.
  INSETS_BOOKMARKS_BAR_BUTTON = views::VIEWS_INSETS_END,
  // Margins used by toasts.
  INSETS_TOAST,
  // Padding used in an omnibox pill button.
  INSETS_OMNIBOX_PILL_BUTTON,
  // Padding used in an page info hover button.
  INSETS_PAGE_INFO_HOVER_BUTTON,
  // Margins for the avatars in the Recent Activity dialog.
  INSETS_RECENT_ACTIVITY_IMAGE_MARGIN,
  // Margins for the contents inside in the Task Manager.
  INSETS_TASK_MANAGER,
  // Padding used in the page info footer button.
  INSETS_PAGE_INFO_FOOTER_BUTTON,
};

enum ChromeDistanceMetric {
  // Default minimum width of a button.
  DISTANCE_BUTTON_MINIMUM_WIDTH = views::VIEWS_DISTANCE_END,
  // Size of the Collaboration Messaging fallback icon padding.
  DISTANCE_COLLABORATION_MESSAGING_AVATAR_FALLBACK_ICON_PADDING,
  // Size of the Collaboration Messaging fallback icon border.
  DISTANCE_COLLABORATION_MESSAGING_AVATAR_FALLBACK_ICON_BORDER_SIZE,
  // Vertical spacing at the beginning and end of a content list (a vertical
  // stack of composite views that behaves like a menu) containing one item.
  DISTANCE_CONTENT_LIST_VERTICAL_SINGLE,
  // Same as |DISTANCE_CONTENT_LIST_VERTICAL_SINGLE|, but used at the beginning
  // and end of a multi-item content list.
  DISTANCE_CONTENT_LIST_VERTICAL_MULTI,
  // Width of the extensions menu.
  DISTANCE_EXTENSIONS_MENU_WIDTH,
  // Width and height of a button's icon in the extensions menu.
  DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE,
  // Width and height of a small button's icon in the extensions menu.
  DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SMALL_SIZE,
  // Width and height of an extension's icon in the extensions menu. This are
  // larger than menu button's icons because it contains internal padding to
  // provide space for badging.
  DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE,
  // Size difference between the two types of icons in the menu. This is used as
  // horizontal and vertical margins to align extensions menu rows.
  DISTANCE_EXTENSIONS_MENU_ICON_SPACING,
  // Vertical and horizontal margin for menu buttons.
  DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN,
  // Horizontal spacing between a label and an icon in the extension's menu
  // entry.
  DISTANCE_EXTENSIONS_MENU_LABEL_ICON_SPACING,
  // Smaller horizontal spacing between other controls that are logically
  // related.
  DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL,
  // Smaller vertical spacing between controls that are logically related.
  DISTANCE_RELATED_CONTROL_VERTICAL_SMALL,
  // Horizontal spacing between an item and the related label, in the context of
  // a row of such items. E.g. the bookmarks bar.
  DISTANCE_RELATED_LABEL_HORIZONTAL_LIST,
  // Horizontal indent of a subsection relative to related items above, e.g.
  // checkboxes below explanatory text/headings.
  DISTANCE_SUBSECTION_HORIZONTAL_INDENT,
  // Vertical margin for controls in a toast.
  DISTANCE_TOAST_CONTROL_VERTICAL,
  // Vertical margin for labels in a toast.
  DISTANCE_TOAST_LABEL_VERTICAL,
  // Larger horizontal spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE,
  // Larger vertical spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE,
  // Width of a bubble that appears mid-screen (like a standalone dialog)
  // instead of being anchored.
  DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH,
  // Horizontal spacing between value and description in the row.
  DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL,
  // Vertical padding at the top and bottom of the an omnibox match row.
  DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING,
  // Vertical padding at the top and bottom of the an omnibox match row for two
  // line layout.
  DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING,
  // Width and Height of a vector icon in the side panel header.
  DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE,
  // Minimum size of the header vector icon buttons to get the proper ripple.
  DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE,
  // Horizontal spacing for separating side panel header border from controls.
  DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL,
  // Horizontal padding between separator in the page info view.
  DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW,
  // Horizontal padding applied between the icon and label in the infobar.
  DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING,
  // Height of info bars.
  DISTANCE_INFOBAR_HEIGHT,
  // Horizontal padding applied between the icon and label in the permission
  // prompt.
  DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING,
  // Horizontal spacing between icon and label in the rich hover button.
  DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL,
  // Horizontal spacing for the search bar's magnifying glass icon and x button.
  DISTANCE_TASK_MANAGER_SEARCH_BAR_ICON_AND_BUTTON_HORIZONTAL_SPACING,
  // Width and height of the vector icons shown in the search bar of the task
  // manager.
  DISTANCE_TASK_MANAGER_SEARCH_ICON_SIZE,
  // The minimum width for the search bar found in Task Manager.
  DISTANCE_TASK_MANAGER_SEARCH_BAR_MIN_WIDTH,
  // The minimum height for the search bar found in Task Manager.
  DISTANCE_TASK_MANAGER_SEARCH_BAR_MIN_HEIGHT,
  // Height of Task Manager tabs.
  DISTANCE_TASK_MANAGER_TAB_HEIGHT,
  // Distance between most child elements inside the toast.
  DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING,
  // Distance between the toast label and action button.
  DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING,
  // Distance between the toast label and the menu button.
  DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_MENU_BUTTON_SPACING,
  // Height of the toast.
  DISTANCE_TOAST_BUBBLE_HEIGHT,
  // Height of toast action buttons.
  DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON,
  // Height of the toast text and close button icon.
  DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT,
  // Width and height of the vector icons shown in the toast bubble.
  DISTANCE_TOAST_BUBBLE_ICON_SIZE,
  // Width and height of the icon that shows the "further options" menu.
  DISTANCE_TOAST_BUBBLE_MENU_ICON_SIZE,
  // Left and right margins of the leading vector icon shown in the toast
  // bubble.
  DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS,
  // Distance between left border of the toast and the icon.
  DISTANCE_TOAST_BUBBLE_MARGIN_LEFT,
  // Distance between the right border of the toast and the action button, if
  // the action button is the rightmost element.
  DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON,
  // Distance between the right border of the toast and the close button, if the
  // close button is the rightmost element.
  DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON,
  // Distance between the right border of the toast and the menu button, if the
  // menu button is the rightmost element.
  DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_MENU_BUTTON,
  // Distance between the right border of the toast and the label, if the label
  // is the rightmost element.
  DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL,
  // Minimum distance between the horizontal edges of the toast and the browser
  // window. Relevant if the toast is wide relative to the browser.
  DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN,
  // Size to resize avatars to in the Recent Activity dialog.
  DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE,
  // Size to use for avatar fallback icon in the Recent Activity dialog.
  DISTANCE_RECENT_ACTIVITY_AVATAR_FALLBACK_SIZE,
  // Size to use for the radius of activity containers in the Recent
  // Activity dialog.
  DISTANCE_RECENT_ACTIVITY_CONTAINER_RADIUS,
  // Size to use for the margin between Recent Activity containers.
  DISTANCE_RECENT_ACTIVITY_CONTAINER_VERTICAL_MARGIN,
  // Additional margin for leading and trailing rows within the Recent
  // Activity dialog.
  DISTANCE_RECENT_ACTIVITY_CONTAINER_VERTICAL_PADDING,
  // Width of the empty border around favicon containers in the Recent Activity
  // dialog.
  DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_BORDER_WIDTH,
  // Distance to offset favicon containers from the avatar in the Recent
  // Activity dialog.
  DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_OFFSET_FROM_AVATAR,
  // Width of the padding inside favicon containers in the Recent Activity
  // dialog.
  DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_PADDING,
  // Size to use for favicon containers in the Recent Activity dialog.
  DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_RADIUS,
  // Vertical padding for rows within the Recent Activity dialog.
  DISTANCE_RECENT_ACTIVITY_ROW_VERTICAL_PADDING,
  // Distance between the avatar icon and the email in the account info row.
  DISTANCE_ACCOUNT_INFO_ROW_AVATAR_EMAIL,
  // Vertical spacing between a textfield and an account card, usually
  // consisting of an avatar icon, name and email address.
  DISTANCE_TEXTFIELD_ACCOUNT_CARD_VERTICAL,
  // Width and height of the vector icon shown in infoboxes in the FFR dialog.
  DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ICON_SIZE,
  // Vertical and horizontal padding of the infoboxes in the FFR dialog.
  DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_PADDING,
  // Rounded corner radius for infoboxes in the FFR dialog.
  DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ROUNDED_BORDER_RADIUS,
  // Vertical spacing between infoboxes in the FFR dialog.
  DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_VERTICAL,
};

class ChromeLayoutProvider : public views::LayoutProvider {
 public:
  ChromeLayoutProvider();

  ChromeLayoutProvider(const ChromeLayoutProvider&) = delete;
  ChromeLayoutProvider& operator=(const ChromeLayoutProvider&) = delete;

  ~ChromeLayoutProvider() override;

  static ChromeLayoutProvider* Get();
  static std::unique_ptr<views::LayoutProvider> CreateLayoutProvider();

  // views::LayoutProvider:
  gfx::Insets GetInsetsMetric(int metric) const override;
  int GetDistanceMetric(int metric) const override;
  int GetSnappedDialogWidth(int min_width) const override;
  const views::TypographyProvider& GetTypographyProvider() const override;

  // Returns whether to show the icon next to the title text on a dialog.
  virtual bool ShouldShowWindowIcon() const;

 private:
  const ChromeTypographyProvider typography_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
