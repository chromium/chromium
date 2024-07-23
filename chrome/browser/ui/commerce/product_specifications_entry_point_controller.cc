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
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service.h"

namespace {

// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to stay valid.
constexpr int kEligibleWindowUrlCountForValidation = 2;
// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to trigger for navigation.
constexpr int kEligibleWindowUrlCountForNavigationTriggering = 3;
// The maximum enforced interval (in days) between two triggering of the entry
// point.
constexpr int kMaxEntryPointTriggeringInterval = 64;

bool CheckWindowContainsEntryPointURLs(
    TabStripModel* tab_strip_model,
    commerce::EntryPointInfo entry_point_info,
    size_t threshold) {
  std::map<GURL, uint64_t> similar_products =
      entry_point_info.similar_candidate_products;
  if (similar_products.size() < threshold) {
    return false;
  }
  std::set<uint64_t> similar_product_ids;
  for (int i = 0; i < tab_strip_model->count(); i++) {
    GURL tab_url = tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL();
    if (similar_products.find(tab_url) != similar_products.end()) {
      similar_product_ids.insert(similar_products[tab_url]);
      if (similar_product_ids.size() >= threshold) {
        return true;
      }
    }
  }
  return similar_product_ids.size() >= threshold;
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
  shopping_service_ =
      ShoppingServiceFactory::GetForBrowserContext(browser->profile());
  if (shopping_service_) {
    product_specifications_service_ =
        shopping_service_->GetProductSpecificationsService();
    cluster_manager_ = shopping_service_->GetClusterManager();
    if (cluster_manager_) {
      cluster_manager_observations_.Observe(cluster_manager_);
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
  const GURL old_url = selection.old_contents->GetLastCommittedURL();
  const GURL new_url = selection.new_contents->GetLastCommittedURL();

  cluster_manager_->GetEntryPointInfoForSelection(
      old_url, new_url,
      base::BindOnce(&ProductSpecificationsEntryPointController::
                         CheckEntryPointInfoForSelection,
                     weak_ptr_factory_.GetWeakPtr(), old_url, new_url));
}

void ProductSpecificationsEntryPointController::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabChangeType change_type) {
  if (change_type == TabChangeType::kAll) {
    // TODO(b/343109556): Instead of hiding, sometimes we'll need to update the
    // showing entry point.
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
  // Reset entry point show gap time.
  browser_->profile()->GetPrefs()->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays, 0);
  DCHECK(product_specifications_service_);
  std::set<GURL> urls;
  auto candidate_products =
      current_entry_point_info_->similar_candidate_products;
  for (auto url_info : shopping_service_->GetUrlInfosForActiveWebWrappers()) {
    if (base::Contains(candidate_products, url_info.url)) {
      urls.insert(url_info.url);
    }
  }
  std::vector<GURL> urls_in_set(urls.begin(), urls.end());
  const std::optional<ProductSpecificationsSet> set =
      product_specifications_service_->AddProductSpecificationsSet(
          current_entry_point_info_->title, std::move(urls_in_set));
  if (set.has_value()) {
    chrome::AddTabAt(browser_,
                     commerce::GetProductSpecsTabUrlForID(set->uuid()),
                     browser_->tab_strip_model()->count(), true, std::nullopt);
  }
}

void ProductSpecificationsEntryPointController::OnEntryPointDismissed() {
  DCHECK(current_entry_point_info_.has_value());
  current_entry_point_info_.reset();

  auto* prefs = browser_->profile()->GetPrefs();
  int current_gap_time = prefs->GetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays);
  // Double the gap time for every dismiss, starting from one day.
  if (current_gap_time == 0) {
    current_gap_time = 1;
  } else {
    current_gap_time =
        std::min(2 * current_gap_time, kMaxEntryPointTriggeringInterval);
  }
  prefs->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays,
      current_gap_time);
  prefs->SetTime(commerce::kProductSpecificationsEntryPointLastDismissedTime,
                 base::Time::Now());
}

void ProductSpecificationsEntryPointController::OnEntryPointHidden() {
  DCHECK(current_entry_point_info_.has_value());
  current_entry_point_info_.reset();
}

bool ProductSpecificationsEntryPointController::ShouldExecuteEntryPointShow() {
  DCHECK(current_entry_point_info_.has_value());
  GURL current_url = browser_->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL();
  std::map<GURL, uint64_t> candidate_products =
      current_entry_point_info_->similar_candidate_products;
  return base::Contains(candidate_products, current_url);
}

void ProductSpecificationsEntryPointController::OnClusterFinishedForNavigation(
    const GURL& url) {
  // Cluster finished for a navigation that didn't happen in this window, or the
  // clustering took so long to finish that the user has navigated away.
  GURL current_url = browser_->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL();
  if (current_url != url || !cluster_manager_) {
    return;
  }

  cluster_manager_->GetEntryPointInfoForNavigation(
      url, base::BindOnce(&ProductSpecificationsEntryPointController::
                              CheckEntryPointInfoForNavigation,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsEntryPointController::CheckEntryPointInfoForSelection(
    const GURL old_url,
    const GURL new_url,
    std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  std::map<GURL, uint64_t> similar_products =
      entry_point_info->similar_candidate_products;
  if (similar_products.find(old_url) == similar_products.end() ||
      similar_products.find(new_url) == similar_products.end()) {
    return;
  }
  if (similar_products[old_url] == similar_products[new_url]) {
    return;
  }

  // Skip server-side check unless specified by feature param.
  if (kProductSpecificationsUseServerClustering.Get()) {
    // TODO(qinmin): we should check whether tabstrips have changed while
    // waiting for the callback.
    cluster_manager_->GetComparableProducts(
        entry_point_info.value(),
        base::BindOnce(&ProductSpecificationsEntryPointController::
                           ShowEntryPointWithTitleForSelection,
                       weak_ptr_factory_.GetWeakPtr(), old_url, new_url));
  } else {
    ShowEntryPointWithTitle(std::move(entry_point_info));
  }
}

void ProductSpecificationsEntryPointController::
    ShowEntryPointWithTitleForSelection(
        const GURL old_url,
        const GURL new_url,
        std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  std::map<GURL, uint64_t> similar_products =
      entry_point_info->similar_candidate_products;
  if (similar_products.find(old_url) == similar_products.end() ||
      similar_products.find(new_url) == similar_products.end()) {
    return;
  }
  ShowEntryPointWithTitle(std::move(entry_point_info));
}

void ProductSpecificationsEntryPointController::
    CheckEntryPointInfoForNavigation(
        std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  if (!IsNavigationEligibleForEntryPoint(browser_->tab_strip_model(),
                                         entry_point_info.value())) {
    return;
  }

  // Skip server-side check unless specified by feature param.
  if (kProductSpecificationsUseServerClustering.Get()) {
    // TODO(qinmin): we should check whether tabstrips have changed while
    // waiting for the callback.
    cluster_manager_->GetComparableProducts(
        entry_point_info.value(),
        base::BindOnce(&ProductSpecificationsEntryPointController::
                           ShowEntryPointWithTitleForNavigation,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    ShowEntryPointWithTitle(std::move(entry_point_info));
  }
}

void ProductSpecificationsEntryPointController::
    ShowEntryPointWithTitleForNavigation(
        std::optional<EntryPointInfo> entry_point_info) {
  if (!entry_point_info.has_value()) {
    return;
  }

  if (!IsNavigationEligibleForEntryPoint(browser_->tab_strip_model(),
                                         entry_point_info.value())) {
    return;
  }
  ShowEntryPointWithTitle(std::move(entry_point_info));
}

void ProductSpecificationsEntryPointController::ShowEntryPointWithTitle(
    std::optional<EntryPointInfo> entry_point_info) {
  auto* prefs = browser_->profile()->GetPrefs();
  int current_gap_time = prefs->GetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays);
  // Back off triggering if gap time is not finished yet.
  if (base::Time::Now() -
          prefs->GetTime(
              commerce::kProductSpecificationsEntryPointLastDismissedTime) <=
      base::Days(current_gap_time)) {
    return;
  }
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
