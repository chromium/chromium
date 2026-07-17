// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SECTION_ACTION_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SECTION_ACTION_ITEM_H_

#include <string>

#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"

// An ActionItem subclass representing structural hierarchy headers in the
// app menu (e.g., "Your Chrome") that cannot be invoked or executed directly.
class AppMenuSectionActionItem : public actions::ActionItem {
  METADATA_HEADER(AppMenuSectionActionItem, actions::ActionItem)
 public:
  explicit AppMenuSectionActionItem(const std::u16string& text);
  AppMenuSectionActionItem(const AppMenuSectionActionItem&) = delete;
  AppMenuSectionActionItem& operator=(const AppMenuSectionActionItem&) = delete;
  ~AppMenuSectionActionItem() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SECTION_ACTION_ITEM_H_
