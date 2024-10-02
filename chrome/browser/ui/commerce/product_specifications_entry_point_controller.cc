// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

namespace {

// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to stay valid.
constexpr int kEligibleWindowUrlCountForValidation = 2;
// Number of URLs of the same cluster that a window needs to contain in order
// for the entry point to trigger for navigation.
constexpr int kEligibleWindowUrlCountForNavigationTriggering = 3;
// The maximum length of the entry point title.
constexpr int kEntryPointTitleMaxLength = 24;

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
    ProductSpecificationsEntryPointController(BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
  if (browser_->GetProfile()->IsRegularProfile()) {
    browser_->GetTabStripModel()->AddObserver(this);
  }
  shopping_service_ =
      ShoppingServiceFactory::GetForBrowserContext(browser_->GetProfile());
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
  if (selection.active_tab_changed() &&
      ProductSpecificationsDisclosureDialog::CloseDialog()) {
    // Don't try to re-trigger the entry point when the dialog is closed due
    // to this tab model change.
    return;
  }

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
  std::set<GURL> urls;
  auto candidate_products =
      current_entry_point_info_->similar_candidate_products;
  for (auto url_info : shopping_service_->GetUrlInfosForActiveWebWrappers()) {
    if (base::Contains(candidate_products, url_info.url)) {
      urls.insert(url_info.url);
    }
  }
  std::vector<GURL> urls_in_set(urls.begin(), urls.end());
  auto* prefs = browser_->GetProfile()->GetPrefs();
  if (!prefs) {
    return;
  }
  // If user has not accepted the latest disclosure, show the disclosure dialog
  // first.
  if (prefs->GetInteger(kProductSpecificationsAcceptedDisclosureVersion) !=
      static_cast<int>(shopping_service::mojom::
                           ProductSpecificationsDisclosureVersion::kV1)) {
    DialogArgs dialog_args(urls_in_set, current_entry_point_info_->title,
                           /*set_id=*/"",
                           /*in_new_tab=*/true);
    ProductSpecificationsDisclosureDialog::ShowDialog(
        browser_->GetProfile(),
        browser_->GetTabStripModel()->GetActiveWebContents(),
        std::move(dialog_args));
    return;
  }
  // Reset entry point show gap time.
  browser_->GetProfile()->GetPrefs()->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays, 0);
  std::vector<commerce::UrlInfo> url_infos;
  for (const auto& url : urls_in_set) {
    url_infos.emplace_back(url, std::u16string());
  }
  const std::optional<ProductSpecificationsSet> set =
      product_specifications_service_->AddProductSpecificationsSet(
          current_entry_point_info_->title, std::move(url_infos));
  if (set.has_value()) {
    // TODO(https://issues.chromium.org/issues/365046217)
    // Have to downcast here to get this to work. Migration from Browser* to
    // BrowserWindowInterface* in progress.
    chrome::AddTabAt(static_cast<Browser*>(browser_),
                     commerce::GetProductSpecsTabUrlForID(set->uuid()),
                     browser_->GetTabStripModel()->count(), true, std::nullopt);
  }
}

void ProductSpecificationsEntryPointController::OnEntryPointDismissed() {
  DCHECK(current_entry_point_info_.has_value());
  current_entry_point_info_.reset();

  auto* prefs = browser_->GetProfile()->GetPrefs();
  int current_gap_time_days = prefs->GetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays);
  // Double the gap time for every dismiss, starting from one day.
  if (current_gap_time_days == 0) {
    current_gap_time_days = 1;
  } else {
    current_gap_time_days = std::min(
        2 * current_gap_time_days, kProductSpecMaxEntryPointTriggeringInterval);
  }
  base::UmaHistogramCounts100("Commerce.Compare.ProactiveBackoffDuration",
                              current_gap_time_days);
  prefs->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays,
      current_gap_time_days);
  prefs->SetTime(commerce::kProductSpecificationsEntryPointLastDismissedTime,
                 base::Time::Now());
}

void ProductSpecificationsEntryPointController::OnEntryPointHidden() {
  DCHECK(current_entry_point_info_.has_value());
  current_entry_point_info_.reset();
}

bool ProductSpecificationsEntryPointController::ShouldExecuteEntryPointShow() {
  DCHECK(current_entry_point_info_.has_value());
  GURL current_url = browser_->GetTabStripModel()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL();
  std::map<GURL, uint64_t> candidate_products =
      current_entry_point_info_->similar_candidate_products;
  return base::Contains(candidate_products, current_url);
}

void ProductSpecificationsEntryPointController::OnClusterFinishedForNavigation(
    const GURL& url) {
  // Cluster finished for a navigation that didn't happen in this window.
  if (!browser_->IsActive()) {
    return;
  }

  cluster_manager_->GetEntryPointInfoForNavigation(
      url, base::BindOnce(&ProductSpecificationsEntryPointController::
                              CheckEntryPointInfoForNavigation,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsEntryPointController::DidFinishNavigation(
    content::WebContents* contents) {
  // TODO(b/343109556): Instead of hiding, sometimes we'll need to update
  // the showing entry point.
  MaybeHideEntryPoint();
  if (contents == browser_->GetTabStripModel()->GetActiveWebContents()) {
    ProductSpecificationsDisclosureDialog::CloseDialog();
  }
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
  base::UmaHistogramEnumeration("Commerce.Compare.CandidateClusterIdentified",
                                CompareEntryPointTrigger::FROM_SELECTION);
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
    base::RecordAction(
        base::UserMetricsAction("Commerce.Compare.CandidateClusterRejected"));
    return;
  }

  std::map<GURL, uint64_t> similar_products =
      entry_point_info->similar_candidate_products;
  if (similar_products.find(old_url) == similar_products.end() ||
      similar_products.find(new_url) == similar_products.end()) {
    base::RecordAction(
        base::UserMetricsAction("Commerce.Compare.CandidateClusterRejected"));
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

  if (!IsNavigationEligibleForEntryPoint(browser_->GetTabStripModel(),
                                         entry_point_info.value())) {
    return;
  }
  base::UmaHistogramEnumeration("Commerce.Compare.CandidateClusterIdentified",
                                CompareEntryPointTrigger::FROM_NAVIGATION);
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
    base::RecordAction(
        base::UserMetricsAction("Commerce.Compare.CandidateClusterRejected"));
    return;
  }

  if (!IsNavigationEligibleForEntryPoint(browser_->GetTabStripModel(),
                                         entry_point_info.value())) {
    base::RecordAction(
        base::UserMetricsAction("Commerce.Compare.CandidateClusterRejected"));
    return;
  }
  ShowEntryPointWithTitle(std::move(entry_point_info));
}

void ProductSpecificationsEntryPointController::ShowEntryPointWithTitle(
    std::optional<EntryPointInfo> entry_point_info) {
  // Using the entry point UI will initiate a data fetch for the product
  // specifications feature. If we're not allowed to fetch this data, don't
  // offer the entry point.
  if (!CanFetchProductSpecificationsData(
          shopping_service_->GetAccountChecker())) {
    return;
  }

  // Entry point should never show for windows with non-regular profile.
  if (!browser_->GetProfile()->IsRegularProfile()) {
    return;
  }

  auto* prefs = browser_->GetProfile()->GetPrefs();
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
  // Show the default title if the set title is too long.
  std::u16string title =
      entry_point_info->title.size() > kEntryPointTitleMaxLength
          ? l10n_util::GetStringUTF16(IDS_COMPARE_ENTRY_POINT_DEFAULT)
          : l10n_util::GetStringFUTF16(
                IDS_COMPARE_ENTRY_POINT,
                base::UTF8ToUTF16(entry_point_info->title));
  base::UmaHistogramCounts100(
      "Commerce.Compare.CandidateClusterSizeWhenShown",
      entry_point_info->similar_candidate_products.size());
  for (auto& observer : observers_) {
    observer.ShowEntryPointWithTitle(std::move(title));
  }
}

void ProductSpecificationsEntryPointController::MaybeHideEntryPoint() {
  if (!current_entry_point_info_.has_value() ||
      IsWindowValidForEntryPoint(browser_->GetTabStripModel(),
                                 current_entry_point_info_.value())) {
    return;
  }
  for (auto& observer : observers_) {
    observer.HideEntryPoint();
  }
}
}  // namespace commerce
