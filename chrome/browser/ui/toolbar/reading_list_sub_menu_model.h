// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_READING_LIST_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_READING_LIST_SUB_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

// A menu model that builds the contents of "Reading List" submenu, which
// includes "Add tab to Reading List" and "Show Reading List" entries.
class ReadingListSubMenuModel : public ui::SimpleMenuModel {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kReadingListMenuShowUI);

  explicit ReadingListSubMenuModel(ui::SimpleMenuModel::Delegate* delegate);

  ReadingListSubMenuModel(const ReadingListSubMenuModel&) = delete;
  ReadingListSubMenuModel& operator=(const ReadingListSubMenuModel&) = delete;

  ~ReadingListSubMenuModel() override;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_READING_LIST_SUB_MENU_MODEL_H_
