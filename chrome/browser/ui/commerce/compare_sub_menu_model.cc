// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/compare_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/commerce/add_to_comparison_table_sub_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"

namespace commerce {

CompareSubMenuModel::CompareSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser,
    ProductSpecificationsService* product_specs_service)
    : SimpleMenuModel(delegate) {
  add_to_comparison_table_sub_menu_model_ =
      std::make_unique<AddToComparisonTableSubMenuModel>(browser,
                                                         product_specs_service);

  AddSubMenuWithStringId(IDC_ADD_TO_COMPARISON_TABLE_MENU,
                         IDS_COMPARE_ADD_TAB_TO_COMPARISON_TABLE,
                         add_to_comparison_table_sub_menu_model_.get());
  AddItemWithStringId(IDC_SHOW_ALL_COMPARISON_TABLES,
                      IDS_COMPARE_SHOW_ALL_COMPARISON_TABLES);
}

CompareSubMenuModel::~CompareSubMenuModel() = default;

}  // namespace commerce
