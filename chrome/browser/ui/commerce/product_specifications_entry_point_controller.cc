// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

#include "base/functional/bind.h"
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

// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to stay valid.
constexpr int kEligibleWindowUrlCountForValidation = 2;
// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to trigger for navigation.
constexpr int kEligibleWindowUrlCountForNavigationTriggering = 3;

bool CheckWindowContainsEntryPointURLs(
    TabStripModel* tab_strip_model,
    commerce::EntryPointInfo entry_point_info,
    size_t threshold) {
  std::set<GURL> similar_urls =
      entry_point_info.similar_candidate_products_urls;
  if (similar_urls.size() < threshold) {
    return false;
  }
  std::set<GURL> eligible_urls_in_current_window;
  for (int i = 0; i < tab_strip_model->count(); i++) {
    GURL tab_url = tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL();
    if (similar_urls.find(tab_url) != similar_urls.end()) {
      eligible_urls_in_current_window.insert(tab_url);
      if (eligible_urls_in_current_window.size() >= threshold) {
        return true;
      }
    }
  }
  return eligible_urls_in_current_window.size() >= threshold;
}

bool IsWindowValidForEntryPoint(TabStripModel* tab_strip_model,
                                commerce::EntryPointInfo entry_point_info) {
  return CheckWindowContainsEntryPointURLs(
      tab_strip_model, entry_point_info, kEligibleWindowUrlCountForValidation);
}

bool IsNavigationEligibleForEntryPoint(
    TabStripModel* tab_strip_model,
    commerce::EntryPointInfo entry_point_info) {
  return CheckWindowContainsEntryPointURLs(
      tab_strip_model, entry_point_info,
      kEligibleWindowUrlCountForNavigationTriggering);
}

}  // namespace

namespace commerce {
// TODO(b/340252809): No need to have browser as a dependency.
ProductSpecificationsEntryPointController::
    ProductSpecificationsEntryPointController(Browser* browser)
    : browser_(browser) {
  CHECK(browser_);
  browser->tab_strip_model()->AddObserver(this);
  ShoppingService* shopping_service =
      ShoppingServiceFactory::GetForBrowserContext(browser->profile());
  if (shopping_service) {
    product_specifications_service_ =
        shopping_service->GetProductSpecificationsService();
    cluster_manager_ = shopping_service->GetClusterManager();
    if (cluster_manager_) {
      cluster_manager_->AddObserver(this);
    }
  }
}

ProductSpecificationsEntryPointController::
    ~ProductSpecificationsEntryPointController() = default;

void ProductSpecificationsEntryPointController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::Type::kRemoved) {
    MaybeHideEntryPoint();
  }
  // Filter out non-tab-selection events.
  if (change.type() != TabStripModelChange::Type::kSelectionOnly ||
      !selection.active_tab_changed() || !selection.old_contents ||
      !selection.new_contents || !cluster_manager_) {
    return;
  }

  cluster_manager_->GetEntryPointInfoForSelection(
      selection.old_contents->GetLastCommittedURL(),
      selection.new_contents->GetLastCommittedURL(),
      base::BindOnce(&ProductSpecificationsEntryPointController::
                         ShowEntryPointWithTitleForSelection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsEntryPointController::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabChangeType change_type) {
  if (change_type == TabChangeType::kAll &&
      contents->GetLastCommittedURL() != last_committed_url_) {
    last_committed_url_ = contents->GetLastCommittedURL();
    MaybeHideEntryPoint();
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
  if (last_committed_url_ != url || !cluster_manager_) {
    return;
  }

  cluster_manager_->GetEntryPointInfoForNavigation(
      url, base::BindOnce(&ProductSpecificationsEntryPointController::
                              ShowEntryPointWithTitleForNavigation,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsEntryPointController::
    ShowEntryPointWithTitleForSelection(
        std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  // TODO(qinmin): we should check whether tabstrips have changed while
  // waiting for the callback.
  ShowEntryPointWithTitle(std::move(entry_point_info));
}

void ProductSpecificationsEntryPointController::
    ShowEntryPointWithTitleForNavigation(
        std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  // TODO(qinmin): we should check whether tabstrips have changed while
  // waiting for the callback.
  if (!IsNavigationEligibleForEntryPoint(browser_->tab_strip_model(),
                                         entry_point_info.value())) {
    return;
  }
  ShowEntryPointWithTitle(std::move(entry_point_info));
}

void ProductSpecificationsEntryPointController::ShowEntryPointWithTitle(
    std::optional<EntryPointInfo> entry_point_info) {
  current_entry_point_info_ = entry_point_info;
  for (auto& observer : observers_) {
    observer.ShowEntryPointWithTitle(entry_point_info->title);
  }
}

void ProductSpecificationsEntryPointController::MaybeHideEntryPoint() {
  if (!current_entry_point_info_.has_value() ||
      IsWindowValidForEntryPoint(browser_->tab_strip_model(),
                                 current_entry_point_info_.value())) {
    return;
  }
  for (auto& observer : observers_) {
    observer.HideEntryPoint();
  }
}
}  // namespace commerce
