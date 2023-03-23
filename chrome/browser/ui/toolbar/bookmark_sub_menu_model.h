// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_

// For views and cocoa, we have complex delegate systems to handle
// injecting the bookmarks to the bookmark submenu. This is done to support
// advanced interactions with the menu contents, like right click context menus.

#include "ui/base/models/simple_menu_model.h"

class Browser;

class BookmarkSubMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kShowBookmarkBarMenuItem);

  BookmarkSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                       Browser* browser);

  BookmarkSubMenuModel(const BookmarkSubMenuModel&) = delete;
  BookmarkSubMenuModel& operator=(const BookmarkSubMenuModel&) = delete;

  ~BookmarkSubMenuModel() override;

 private:
  void Build(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
