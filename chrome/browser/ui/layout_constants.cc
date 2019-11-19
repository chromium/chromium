// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"

#if defined(OS_MACOSX)
int GetCocoaLayoutConstant(LayoutConstant constant) {
  switch (constant) {
    case BOOKMARK_BAR_HEIGHT:
      return 28;
    case BOOKMARK_BAR_NTP_HEIGHT:
      return 39;
    case BOOKMARK_BAR_HEIGHT_NO_OVERLAP:
      return GetCocoaLayoutConstant(BOOKMARK_BAR_HEIGHT) - 2;
    case BOOKMARK_BAR_NTP_PADDING:
      return (GetCocoaLayoutConstant(BOOKMARK_BAR_NTP_HEIGHT) -
              GetCocoaLayoutConstant(BOOKMARK_BAR_HEIGHT)) /
             2;
    default:
      return GetLayoutConstant(constant);
  }
}
#endif

int GetLayoutConstant(LayoutConstant constant) {
  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  switch (constant) {
    case BOOKMARK_BAR_HEIGHT:
      // The fixed margin ensures the bookmark buttons appear centered relative
      // to the white space above and below.
      static constexpr int kBookmarkBarAttachedVerticalMargin = 4;
      return GetLayoutConstant(BOOKMARK_BAR_BUTTON_HEIGHT) +
             kBookmarkBarAttachedVerticalMargin;
    case BOOKMARK_BAR_BUTTON_HEIGHT:
      return touch_ui ? 36 : 28;
    case BOOKMARK_BAR_NTP_HEIGHT:
      return touch_ui ? GetLayoutConstant(BOOKMARK_BAR_HEIGHT) : 39;
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
    case LOCATION_BAR_ELEMENT_PADDING:
      return touch_ui ? 3 : 2;
    case LOCATION_BAR_HEIGHT:
      return touch_ui ? 36 : 28;
    case LOCATION_BAR_ICON_SIZE:
      return touch_ui ? 20 : 16;
    case TAB_AFTER_TITLE_PADDING:
      return touch_ui ? 8 : 4;
    case TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH:
      return 16;
    case TAB_ALERT_INDICATOR_ICON_WIDTH:
      return touch_ui ? 12 : 16;
    case TAB_HEIGHT:
      return (touch_ui ? 41 : 34) + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
    case TAB_PRE_TITLE_PADDING:
      return 8;
    case TAB_STACK_DISTANCE:
      return touch_ui ? 4 : 6;
    case TABSTRIP_TOOLBAR_OVERLAP:
      return 1;
    case TOOLBAR_BUTTON_HEIGHT:
      return touch_ui ? 48 : 28;
    case TOOLBAR_ELEMENT_PADDING:
      return touch_ui ? 0 : 4;
    case TOOLBAR_STANDARD_SPACING:
      return touch_ui ? 12 : 8;
    default:
      break;
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  switch (inset) {
    case LOCATION_BAR_ICON_INTERIOR_PADDING:
      return touch_ui ? gfx::Insets(5, 10) : gfx::Insets(4, 8);

    case TOOLBAR_BUTTON:
      return gfx::Insets(touch_ui ? 12 : 6);

    case TOOLBAR_ACTION_VIEW: {
      // TODO(afakhry): Unify all toolbar button sizes on all platforms.
      // https://crbug.com/822967.
      return gfx::Insets(touch_ui ? 10 : 0);
    }

    case TOOLBAR_INTERIOR_MARGIN:
      return touch_ui ? gfx::Insets() : gfx::Insets(4, 8);
  }
  NOTREACHED();
  return gfx::Insets();
}
