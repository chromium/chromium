// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/shopping_service.h"

namespace {

constexpr int kEligibleWindowUrlCountForNavigation = 3;

bool IsNavigationEligibleForEntryPoint(
    TabStripModel* tab_strip_model,
    commerce::EntryPointInfo entry_point_info) {
  std::set<GURL> similar_urls =
      entry_point_info.similar_candidate_products_urls;
  if (similar_urls.size() < kEligibleWindowUrlCountForNavigation) {
    return false;
  }
  std::set<GURL> eligible_urls_in_current_window;
  for (int i = 0; i < tab_strip_model->count(); i++) {
    GURL tab_url = tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL();
    if (similar_urls.find(tab_url) != similar_urls.end()) {
      eligible_urls_in_current_window.insert(tab_url);
      if (eligible_urls_in_current_window.size() >=
          kEligibleWindowUrlCountForNavigation) {
        return true;
      }
    }
  }
  return eligible_urls_in_current_window.size() >=
         kEligibleWindowUrlCountForNavigation;
}

}  // namespace

namespace commerce {
// TODO(b/340252809): No need to have browser as a dependency.
ProductSpecificationsEntryPointController::
    ProductSpecificationsEntryPointController(Browser* browser)
    : browser_(browser),
      shopping_service_(
          ShoppingServiceFactory::GetForBrowserContext(browser->profile())) {
  CHECK(browser_);
  browser->tab_strip_model()->AddObserver(this);
  if (shopping_service_) {
    product_specifications_service_ =
        shopping_service_->GetProductSpecificationsService();
    shopping_service_->AddClusterManagerObserver(this);
  }
}

ProductSpecificationsEntryPointController::
    ~ProductSpecificationsEntryPointController() = default;

void ProductSpecificationsEntryPointController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Filter out non-tab-selection events.
  if (change.type() != TabStripModelChange::Type::kSelectionOnly ||
      !selection.active_tab_changed() || !selection.old_contents ||
      !selection.new_contents || !shopping_service_) {
    return;
  }

  auto entry_point_info = shopping_service_->GetEntryPointInfoForSelection(
      selection.old_contents->GetLastCommittedURL(),
      selection.new_contents->GetLastCommittedURL());
  if (entry_point_info.has_value()) {
    current_entry_point_info_ = entry_point_info;
    for (auto& observer : observers_) {
      observer.ShowEntryPointWithTitle(entry_point_info->title);
    }
  }
}

void ProductSpecificationsEntryPointController::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabChangeType change_type) {
  if (change_type == TabChangeType::kAll) {
    last_committed_url_ = contents->GetLastCommittedURL();
  }
}

void ProductSpecificationsEntryPointController::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void ProductSpecificationsEntryPointController::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProductSpecificationsEntryPointController::OnEntryPointExecuted() {
  if (!current_entry_point_info_.has_value()) {
    return;
  }
  DCHECK(product_specifications_service_);
  std::vector<GURL> urls(
      current_entry_point_info_->similar_candidate_products_urls.begin(),
      current_entry_point_info_->similar_candidate_products_urls.end());
  const std::optional<ProductSpecificationsSet> set =
      product_specifications_service_->AddProductSpecificationsSet(
          current_entry_point_info_->title, std::move(urls));
  if (set.has_value()) {
    chrome::AddTabAt(browser_,
                     commerce::GetProductSpecsTabUrlForID(set->uuid()),
                     browser_->tab_strip_model()->count(), true, std::nullopt);
  }
}

void ProductSpecificationsEntryPointController::OnEntryPointDismissed() {
  // TODO(b/325661685): Add implementation for back-off mechanism.
}

void ProductSpecificationsEntryPointController::OnEntryPointHidden() {
  DCHECK(current_entry_point_info_.has_value());
  current_entry_point_info_.reset();
}

void ProductSpecificationsEntryPointController::OnClusterFinishedForNavigation(
    const GURL& url) {
  // Cluster finished for a navigation that didn't happen in this window.
  if (last_committed_url_ != url) {
    return;
  }

  auto entry_point_info =
      shopping_service_->GetEntryPointInfoForNavigation(url);
  if (!entry_point_info.has_value() ||
      !IsNavigationEligibleForEntryPoint(browser_->tab_strip_model(),
                                         entry_point_info.value())) {
    return;
  }
  current_entry_point_info_ = entry_point_info;
  for (auto& observer : observers_) {
    observer.ShowEntryPointWithTitle(entry_point_info->title);
  }
}
}  // namespace commerce
