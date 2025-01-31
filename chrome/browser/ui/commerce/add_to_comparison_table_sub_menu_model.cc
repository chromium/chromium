// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/add_to_comparison_table_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/ui_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace commerce {

AddToComparisonTableSubMenuModel::AddToComparisonTableSubMenuModel(
    Browser* browser,
    ProductSpecificationsService* product_specs_service)
    : SimpleMenuModel(this),
      browser_(browser),
      product_specs_service_(product_specs_service),
      next_menu_id_(AppMenuModel::kMinCompareCommandId) {
  AddItemWithStringId(IDC_CREATE_NEW_COMPARISON_TABLE_WITH_TAB,
                      IDS_COMPARE_NEW_COMPARISON_TABLE);

  auto sets = product_specs_service_->GetAllProductSpecifications();
  if (sets.empty()) {
    return;
  }

  AddSeparator(ui::NORMAL_SEPARATOR);

  auto& current_url =
      browser_->GetActiveTabInterface()->GetContents()->GetLastCommittedURL();

  for (const auto& set : sets) {
    const int command_id = GetAndIncrementNextMenuId();
    command_id_to_table_info_[command_id] = {set.uuid(),
                                             set.ContainsUrl(current_url)};
    AddItem(command_id, base::UTF8ToUTF16(set.name()));
  }
}

AddToComparisonTableSubMenuModel::~AddToComparisonTableSubMenuModel() = default;

void AddToComparisonTableSubMenuModel::ExecuteCommand(int command_id,
                                                      int event_flags) {
  auto& current_url =
      browser_->GetActiveTabInterface()->GetContents()->GetLastCommittedURL();
  if (command_id == IDC_CREATE_NEW_COMPARISON_TABLE_WITH_TAB) {
    chrome::OpenCommerceProductSpecificationsTab(
        browser_, {current_url},
        browser_->GetTabStripModel()->GetIndexOfTab(
            browser_->GetActiveTabInterface()));
    return;
  }

  auto& title = browser_->GetActiveTabInterface()->GetContents()->GetTitle();
  const UrlInfo url_info(current_url, title);

  const auto table_info_for_uuid = command_id_to_table_info_.find(command_id);
  AddUrlToSet(url_info, table_info_for_uuid->second.uuid);
}

bool AddToComparisonTableSubMenuModel::IsCommandIdEnabled(
    int command_id) const {
  if (command_id == IDC_CREATE_NEW_COMPARISON_TABLE_WITH_TAB) {
    return true;
  }

  // Disable the table option if it already contains the current URL.
  const auto table_info_for_uuid = command_id_to_table_info_.find(command_id);
  return !table_info_for_uuid->second.contains_current_url;
}

int AddToComparisonTableSubMenuModel::GetAndIncrementNextMenuId() {
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}

void AddToComparisonTableSubMenuModel::AddUrlToSet(const UrlInfo& url_info,
                                                   base::Uuid set_uuid) {
  std::optional<ProductSpecificationsSet> set =
      product_specs_service_->GetSetByUuid(set_uuid);

  // If the set was not found, then it was deleted while the menu was open.
  if (!set.has_value()) {
    return;
  }

  const GURL& url = url_info.url;
  const std::u16string& title = url_info.title;

  std::vector<UrlInfo> existing_url_infos = set->url_infos();
  auto it = std::ranges::find_if(existing_url_infos,
                                 [&url](const UrlInfo& query_url_info) {
                                   return query_url_info.url == url;
                                 });

  // Add the URL to the set. If it is already in the set (because it was added
  // after the menu was opened), then we still show the toast.
  if (it == existing_url_infos.end()) {
    existing_url_infos.emplace_back(url, title);
    product_specs_service_->SetUrls(set_uuid, std::move(existing_url_infos));
  }

  ShowProductSpecsConfirmationToast(base::UTF8ToUTF16(set->name()), browser_);
}

}  // namespace commerce
