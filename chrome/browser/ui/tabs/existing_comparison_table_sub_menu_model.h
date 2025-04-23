// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_COMPARISON_TABLE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_COMPARISON_TABLE_SUB_MENU_MODEL_H_

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"

class TabMenuModelDelegate;

namespace commerce {

// This submenu displays a list of existing comparison tables which do not
// already contain the selected tab's URL. When a menu item is clicked, the tab
// is added to the corresponding comparison table.
class ExistingComparisonTableSubMenuModel : public ExistingBaseSubMenuModel {
 public:
  ExistingComparisonTableSubMenuModel(
      ui::SimpleMenuModel::Delegate* parent_delegate,
      TabMenuModelDelegate* tab_menu_model_delegate,
      TabStripModel* model,
      int context_index,
      ProductSpecificationsService* product_specs_service);
  ExistingComparisonTableSubMenuModel(
      const ExistingComparisonTableSubMenuModel&) = delete;
  ExistingComparisonTableSubMenuModel& operator=(
      const ExistingComparisonTableSubMenuModel&) = delete;
  ~ExistingComparisonTableSubMenuModel() override;

  // Whether the submenu should be shown in the provided context.
  static bool ShouldShowSubmenu(
      const GURL& tab_url,
      ProductSpecificationsService* product_specs_service);

 private:
  // ExistingBaseSubMenuModel
  void ExecuteExistingCommand(size_t target_index) override;

  const std::vector<MenuItemInfo> GetMenuItems(
      const std::vector<ProductSpecificationsSet> sets);
  void AddUrlToSet(const UrlInfo& url_info, base::Uuid set_uuid);

  const raw_ptr<ProductSpecificationsService> product_specs_service_;

  // Map of index in the menu model to table UUID.
  std::map<size_t, base::Uuid> target_index_to_table_uuid_mapping_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_COMPARISON_TABLE_SUB_MENU_MODEL_H_
