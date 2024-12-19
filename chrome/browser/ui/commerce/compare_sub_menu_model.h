// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_

#include "ui/menus/simple_menu_model.h"

namespace commerce {

class CompareSubMenuModel : public ui::SimpleMenuModel {
 public:
  explicit CompareSubMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  CompareSubMenuModel(const CompareSubMenuModel&) = delete;
  CompareSubMenuModel& operator=(const CompareSubMenuModel&) = delete;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_
