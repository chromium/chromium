// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_

#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/user_education/common/menu/highlighting_menu_button_helper.h"
#include "components/user_education/common/menu/highlighting_simple_menu_model_delegate.h"

// In order to have automatic toolbar button menu highlighting:
//  - Derive your model from user_education::HighlightingSimpleMenuModelDelegate
//    instead of ui::SimpleMenuModel::Delegate.
//  - Have a ToolbarButtonMenuHighlighter member of your button object.
//  - Call MaybeHighlight() when your menu is about to be shown.

// Handles closing an attached IPH and possibly highlighting a menu item when a
// toolbar button menu is about to be shown.
class ToolbarButtonMenuHighlighter
    : public user_education::HighlightingMenuButtonHelper {
 public:
  // This is the "nicer" version of `MaybeHighlight()` that should actually be
  // used by toolbar buttons.
  void MaybeHighlight(
      Browser* browser,
      ToolbarButton* button,
      user_education::HighlightingSimpleMenuModelDelegate* menu_model);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_
