// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

int layout_constant_values[LAYOUT_CONSTANTS_MAX_VALUE];

void SetTabStripFullscreen() {
  if (!layout_constant_values[TAB_STRIP_PAD_WHEN_MAXIMIZED])
     layout_constant_values[TAB_STRIP_PADDING] = 0;
}

void SetTabStripWindowed() {
  layout_constant_values[TAB_STRIP_PADDING] = layout_constant_values[TAB_STRIP_MAXIMIZED_ANTI_PADDING];
}


void SetLayoutConstantsFallback() {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  layout_constant_values[APP_MENU_PROFILE_ROW_AVATAR_ICON_SIZE] = 24;
  layout_constant_values[APP_MENU_MAXIMUM_CHARACTER_LENGTH] = 30;
  layout_constant_values[BOOKMARK_BAR_BUTTON_HEIGHT] = touch_ui ? 36 : 28;
  layout_constant_values[BOOKMARK_BAR_HEIGHT] = layout_constant_values[BOOKMARK_BAR_BUTTON_HEIGHT] +
                                                6;
  layout_constant_values[APP_MENU_MAXIMUM_CHARACTER_LENGTH] = 30;
  layout_constant_values[TOOLBAR_ELEMENT_PADDING] = touch_ui ? 0 : 4;
  layout_constant_values[BOOKMARK_BAR_BUTTON_PADDING] = layout_constant_values[TOOLBAR_ELEMENT_PADDING];
  layout_constant_values[BOOKMARK_BAR_BUTTON_IMAGE_LABEL_PADDING] = 6;
  layout_constant_values[WEB_APP_MENU_BUTTON_SIZE] = 24;
  layout_constant_values[WEB_APP_PAGE_ACTION_ICON_SIZE] = 16;
  layout_constant_values[LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING] = 2;
  layout_constant_values[LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET] = 1;
  layout_constant_values[LOCATION_BAR_CHILD_INTERIOR_PADDING] = 3;
  layout_constant_values[LOCATION_BAR_CHILD_CORNER_RADIUS] = 12;
  layout_constant_values[LOCATION_BAR_CHIP_ICON_SIZE] = 16;
  layout_constant_values[LOCATION_BAR_CHIP_PADDING] = 4;
  layout_constant_values[LOCATION_BAR_ELEMENT_PADDING] = touch_ui ? 3 : 2;
  layout_constant_values[LOCATION_BAR_PAGE_INFO_ICON_VERTICAL_PADDING] = touch_ui ? 3 : 5;
  layout_constant_values[LOCATION_BAR_LEADING_DECORATION_EDGE_PADDING] = 0;
  layout_constant_values[LOCATION_BAR_TRAILING_DECORATION_EDGE_PADDING] = touch_ui ? 3 : 12;
  layout_constant_values[LOCATION_BAR_TRAILING_DECORATION_INNER_PADDING] = touch_ui ? 3 : 8;
  layout_constant_values[LOCATION_BAR_HEIGHT] = touch_ui ? 36 : 34;
  layout_constant_values[LOCATION_BAR_ICON_SIZE] = touch_ui ? 20 : 16;
  layout_constant_values[LOCATION_BAR_LEADING_ICON_SIZE] = layout_constant_values[LOCATION_BAR_ICON_SIZE];
  layout_constant_values[LOCATION_BAR_TRAILING_ICON_SIZE] = 20;
  layout_constant_values[TAB_AFTER_TITLE_PADDING] = touch_ui ? 8 : 4;
  layout_constant_values[TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH] = 16;
  layout_constant_values[TAB_ALERT_INDICATOR_ICON_WIDTH] = touch_ui ? 12 : 16;
  layout_constant_values[TAB_CLOSE_BUTTON_SIZE] = touch_ui ? 24 : 16;
  layout_constant_values[TABSTRIP_TOOLBAR_OVERLAP] = base::FeatureList::IsEnabled(tabs::kScrollableTabStrip) ? 0 : 1;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("compact-tab-ui")) {
      layout_constant_values[TAB_HEIGHT] = 28;
      layout_constant_values[TAB_STRIP_PADDING] = 6;
  } else if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "") {
      layout_constant_values[TAB_HEIGHT] = 30;
      layout_constant_values[TAB_STRIP_PADDING] = 6;
  } else {
      layout_constant_values[TAB_HEIGHT] = 34 + layout_constant_values[TABSTRIP_TOOLBAR_OVERLAP];
      layout_constant_values[TAB_STRIP_PADDING] = 6;
  }
  layout_constant_values[TAB_STRIP_HEIGHT] = layout_constant_values[TAB_HEIGHT] + layout_constant_values[TAB_STRIP_PADDING];
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60" ||
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular") {
      layout_constant_values[TAB_SEPARATOR_HEIGHT] = 0;
  } else {
      layout_constant_values[TAB_SEPARATOR_HEIGHT] = touch_ui ? 24 : 20;
      if (base::CommandLine::ForCurrentProcess()->HasSwitch("compact-tab-ui"))
          layout_constant_values[TAB_SEPARATOR_HEIGHT] -= 6;
   }
  layout_constant_values[TAB_PRE_TITLE_PADDING] = 8;
  layout_constant_values[TAB_STACK_DISTANCE] = touch_ui ? 4 : 6;
  layout_constant_values[TOOLBAR_BUTTON_HEIGHT] = touch_ui ? 48 : 34;
  layout_constant_values[TOOLBAR_DIVIDER_CORNER_RADIUS] = 1;
  layout_constant_values[TOOLBAR_DIVIDER_HEIGHT] = touch_ui ? 20 : 16;
  layout_constant_values[TOOLBAR_DIVIDER_SPACING] = 9;
  layout_constant_values[TOOLBAR_DIVIDER_WIDTH] = 2;
  layout_constant_values[TOOLBAR_ICON_DEFAULT_MARGIN] = touch_ui ? 0 : 2;
  layout_constant_values[TOOLBAR_STANDARD_SPACING] = touch_ui ? 12 : 9;
  layout_constant_values[PAGE_INFO_ICON_SIZE] = 20;
  layout_constant_values[DOWNLOAD_ICON_SIZE] = 20;
  layout_constant_values[TOOLBAR_CORNER_RADIUS] = 8;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60" ||
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular") {
      layout_constant_values[TAB_WIDTH] = 193;
  } else {
      layout_constant_values[TAB_WIDTH] = 240;     
  }
  layout_constant_values[TAB_HORIZONTAL_PADDING] = 8;
  layout_constant_values[TAB_VERTICAL_PADDING] = 6;
  layout_constant_values[TAB_TOP_CORNER_RADIUS] = 10;
  layout_constant_values[TAB_BOTTOM_CORNER_RADIUS] = 12;
  layout_constant_values[TAB_STRIP_MAXIMIZED_ANTI_PADDING] = 4;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("compact-tab-ui")) {
      layout_constant_values[TAB_FAVICON_Y_OFFSET] = 8;
  } else if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "") {
      layout_constant_values[TAB_FAVICON_Y_OFFSET] = 10;
  } else {
      layout_constant_values[TAB_FAVICON_Y_OFFSET] = 12;
  }
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "") {
      layout_constant_values[TAB_FAVICON_X_OFFSET] = 12;
  } else {
      layout_constant_values[TAB_FAVICON_X_OFFSET] = 5;
  }
  layout_constant_values[TAB_CLOSE_BUTTON_X_OFFSET] = 8;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60") {
      layout_constant_values[TAB_CLOSE_BUTTON_X_OFFSET] = 14;
  } else {
      layout_constant_values[TAB_CLOSE_BUTTON_X_OFFSET] = 8;
  }
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60") {
      layout_constant_values[TAB_STRIP_PAD_WHEN_MAXIMIZED] = 1;
      layout_constant_values[TAB_OVERLAP] = 11;
  } else if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular") {
      layout_constant_values[TAB_STRIP_PAD_WHEN_MAXIMIZED] = 0;
      layout_constant_values[TAB_OVERLAP] = 8;
  } else if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "cr23") {
      layout_constant_values[TAB_STRIP_PAD_WHEN_MAXIMIZED] = 1;
      layout_constant_values[TAB_OVERLAP] = 12;
  } else {
      layout_constant_values[TAB_STRIP_PAD_WHEN_MAXIMIZED] = 0;
      layout_constant_values[TAB_OVERLAP] = 16;
  }
  layout_constant_values[TAB_SEPARATOR_OFFSET] = 0;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "rectangular" ||
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "v60") {
      layout_constant_values[TAB_HARD_BORDER] = 1;
  } else {
      layout_constant_values[TAB_HARD_BORDER] = 0;
  }
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("supermium-tab-options") == "cr23") {
      layout_constant_values[DRAW_LEFT_TAB_SEPARATOR] = 0;
  } else {
      layout_constant_values[DRAW_LEFT_TAB_SEPARATOR] = 1;
  }
  layout_constant_values[DRAW_RIGHT_TAB_SEPARATOR] = 1;
  layout_constant_values[TAB_CLOSE_BUTTON_Y_OFFSET] = 0;
  layout_constant_values[TAB_TITLE_Y_OFFSET] = 0;
}

bool g_layout_constants_initialized = false;

void SetLayoutConstants() {
  /*
     Due to poor performance reported by some users, the ability to update user metrics "live"
     will be disabled by default. One update close to launch should be sufficient.
  */
  if (g_layout_constants_initialized &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch("enable-advanced-customization"))
      return;

  g_layout_constants_initialized = true;
  SetLayoutConstantsFallback();

  base::FilePath userdir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &userdir))
      return; // Things are seriously wrong if the user data directory cannot be located.
  const base::FilePath userpath = userdir.Append(FILE_PATH_LITERAL("scs"));
  std::string bufstr;
  base::ReadFileToString(userpath, &bufstr);

  if (bufstr.empty()) {
      return;
  }

  for (int i = 0; i < LAYOUT_CONSTANTS_MAX_VALUE; i++)
  {
      // In scs, each layout "constant" is identified by its index in the LayoutConstant enum.
      // This may mean that the index corresponding to a constant may change from version to version, 
      // but it does simplify the parsing process.
	  std::string layout_name = std::string("int_layout_constant_") + std::to_string(i) + std::string("{");
      std::string::size_type layout_setting_pos = bufstr.find(layout_name, 0);
      if (layout_setting_pos == std::string::npos) {
          continue;
      }
      std::string constant_val = bufstr.substr(layout_setting_pos + layout_name.length());
	  constant_val = constant_val.substr(0, constant_val.find("}"));
      if(!std::all_of(constant_val.begin(), constant_val.end(), [](char c) {return c >= '0' && c <= '9';}))
          continue;
      layout_constant_values[i] = std::stoi(constant_val);
  }
  layout_constant_values[TAB_STRIP_HEIGHT] = layout_constant_values[TAB_HEIGHT] + layout_constant_values[TAB_STRIP_PADDING];
}

int GetLayoutConstant(LayoutConstant constant) {
  int result = constant[layout_constant_values];
  return result;
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

    case AVATAR_CHIP_PADDING:
      if (touch_ui) {
        return GetLayoutInsets(TOOLBAR_BUTTON);
      } else {
        return gfx::Insets::TLBR(7, 10, 7, 4);
      }

    case TOOLBAR_INTERIOR_MARGIN:
      return touch_ui ? gfx::Insets() : gfx::Insets::VH(6, 5);

    case WEBUI_TAB_STRIP_TOOLBAR_INTERIOR_MARGIN:
      return gfx::Insets::VH(4, 0);
  }
  NOTREACHED();
}
