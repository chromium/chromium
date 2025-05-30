// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_ADD_TO_COMPARISON_TABLE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_COMMERCE_ADD_TO_COMPARISON_TABLE_SUB_MENU_MODEL_H_

#include "base/uuid.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "ui/menus/simple_menu_model.h"

class Browser;

namespace commerce {

class AddToComparisonTableSubMenuModel : public ui::SimpleMenuModel,
                                         public ui::SimpleMenuModel::Delegate {
 public:
  AddToComparisonTableSubMenuModel(
      Browser* browser,
      commerce::ProductSpecificationsService* product_specs_service);

  AddToComparisonTableSubMenuModel(const AddToComparisonTableSubMenuModel&) =
      delete;
  AddToComparisonTableSubMenuModel& operator=(
      const AddToComparisonTableSubMenuModel&) = delete;

  ~AddToComparisonTableSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  struct TableInfo {
    // UUID of the table.
    base::Uuid uuid;

    // Whether the current URL is already in the table.
    bool contains_current_url;
  };

  int GetAndIncrementNextMenuId();

  void AddUrlToSet(const UrlInfo& url, base::Uuid set_uuid);
  void ShowConfirmationToast(std::u16string set_name);

  raw_ptr<Browser> browser_;
  raw_ptr<ProductSpecificationsService> product_specs_service_;

  int next_menu_id_ = 0;

  std::map<int, TableInfo> command_id_to_table_info_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_ADD_TO_COMPARISON_TABLE_SUB_MENU_MODEL_H_
