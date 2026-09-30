// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_

#include <optional>

#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/menu/highlighting_menu_button_helper.h"
#include "components/user_education/common/menu/highlighting_simple_menu_model_delegate.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;

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
  struct HighlightInfo {
    ui::ElementIdentifier highlighted_menu_identifier;
    user_education::FeaturePromoHandle promo_handle;
  };

  // This is the "nicer" version of `MaybeHighlight()` that should actually be
  // used by toolbar buttons.
  void MaybeHighlight(
      BrowserWindowInterface* browser,
      ToolbarButton* button,
      user_education::HighlightingSimpleMenuModelDelegate* menu_model);

  // Version for action-based menus (such as `ActionAppMenu`) that do not use
  // `ui::SimpleMenuModel`. If an IPH is anchored to `button_element_id`, closes
  // the bubble and continues the promo, returning the highlighted menu item ID
  // and promo handle.
  static std::optional<HighlightInfo> MaybeHighlight(
      BrowserWindowInterface* browser,
      ui::ElementIdentifier button_element_id);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_MENU_HIGHLIGHTER_H_
