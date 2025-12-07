// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_comparison_table_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/commerce/browser_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/ui_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/strings/grit/components_strings.h"

namespace commerce {

ExistingComparisonTableSubMenuModel::ExistingComparisonTableSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* model,
    int context_index,
    ProductSpecificationsService* product_specs_service)
    : ExistingBaseSubMenuModel(
          parent_delegate,
          model,
          context_index,
          kMinExistingComparisonTableCommandId,
          TabStripModel::CommandCommerceProductSpecifications),
      product_specs_service_(product_specs_service) {
  auto sets = product_specs_service_->GetAllProductSpecifications();

  // This submenu shouldn't be created or shown if there are no sets.
  CHECK(!sets.empty());

  Build(IDS_COMPARE_NEW_COMPARISON_TABLE, GetMenuItems(sets));
}

ExistingComparisonTableSubMenuModel::~ExistingComparisonTableSubMenuModel() =
    default;

bool ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
    const GURL& tab_url,
    ProductSpecificationsService* product_specs_service) {
  if (product_specs_service) {
    // Show the submenu if at least one set does not contain the URL.
    auto sets = product_specs_service->GetAllProductSpecifications();
    for (const auto& set : sets) {
      if (!set.ContainsUrl(tab_url)) {
        return true;
      }
    }
  }
  return false;
}

void ExistingComparisonTableSubMenuModel::ExecuteExistingCommand(
    size_t target_index) {
  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!model()->ContainsIndex(GetContextIndex())) {
    return;
  }

  CHECK_LE(size_t(target_index), target_index_to_table_uuid_mapping_.size());

  auto* web_contents = model()->GetTabAtIndex(GetContextIndex())->GetContents();
  const UrlInfo url_info(web_contents->GetLastCommittedURL(),
                         web_contents->GetTitle());

  const auto& uuid_for_target_index =
      target_index_to_table_uuid_mapping_.find(target_index);
  AddUrlToSet(url_info, uuid_for_target_index->second);
}

const std::vector<ExistingComparisonTableSubMenuModel::MenuItemInfo>
ExistingComparisonTableSubMenuModel::GetMenuItems(
    const std::vector<ProductSpecificationsSet> sets) {
  std::vector<MenuItemInfo> menu_item_infos;

  auto& tab_url =
      model()->GetWebContentsAt(GetContextIndex())->GetLastCommittedURL();

  for (const auto& set : sets) {
    // Hide sets that already contain the URL.
    if (set.ContainsUrl(tab_url)) {
      continue;
    }

    menu_item_infos.emplace_back(base::UTF8ToUTF16(set.name()));
    const size_t target_index = menu_item_infos.size() - 1;
    target_index_to_table_uuid_mapping_[target_index] = set.uuid();
    menu_item_infos[target_index].target_index = target_index;
  }

  return menu_item_infos;
}

void ExistingComparisonTableSubMenuModel::AddUrlToSet(const UrlInfo& url_info,
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

  if (auto* toast_controller = ToastController::MaybeGetForWebContents(
          model()->GetTabAtIndex(GetContextIndex())->GetContents())) {
    ShowProductSpecsConfirmationToast(base::UTF8ToUTF16(set->name()),
                                      toast_controller);
  }
}

}  // namespace commerce
