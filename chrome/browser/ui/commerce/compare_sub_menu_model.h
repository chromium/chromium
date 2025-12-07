// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_

#include "chrome/browser/ui/commerce/add_to_comparison_table_sub_menu_model.h"
#include "ui/menus/simple_menu_model.h"

class Browser;

namespace commerce {

class CompareSubMenuModel : public ui::SimpleMenuModel {
 public:
  CompareSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                      Browser* browser,
                      ProductSpecificationsService* product_specs_service);
  CompareSubMenuModel(const CompareSubMenuModel&) = delete;
  CompareSubMenuModel& operator=(const CompareSubMenuModel&) = delete;
  ~CompareSubMenuModel() override;

 private:
  std::unique_ptr<AddToComparisonTableSubMenuModel>
      add_to_comparison_table_sub_menu_model_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_COMPARE_SUB_MENU_MODEL_H_
