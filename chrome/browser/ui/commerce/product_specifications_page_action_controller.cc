// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_page_action_controller.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The maximum length of the page action title.
constexpr int kPageActionTitleMaxLength = 24;
}  // namespace

namespace commerce {

ProductSpecificationsPageActionController::
    ProductSpecificationsPageActionController(
        base::RepeatingCallback<void()> notify_callback,
        ShoppingService* shopping_service)
    : CommercePageActionController(std::move(notify_callback)),
      shopping_service_(shopping_service) {
  if (shopping_service_) {
    product_specifications_service_ =
        shopping_service_->GetProductSpecificationsService();
    if (product_specifications_service_) {
      obs_.Observe(product_specifications_service_);
    }
  }
}

ProductSpecificationsPageActionController::
    ~ProductSpecificationsPageActionController() = default;

std::optional<bool>
ProductSpecificationsPageActionController::ShouldShowForNavigation() {
  // If the user isn't eligible for the feature, don't block.
  if (!shopping_service_ || !shopping_service_->GetAccountChecker() ||
      !commerce::CanFetchProductSpecificationsData(
          shopping_service_->GetAccountChecker())) {
    return false;
  }
  // If the page is not yet determined to be a product page, we're "undecided".
  if (!got_product_response_for_page_) {
    return std::nullopt;
  }
  // If we got a response from the shopping service but the response was empty,
  // we don't need to know about the product group info.
  if (got_product_response_for_page_ && !product_info_for_page_.has_value()) {
    return false;
  }

  if (!product_group_for_page_.has_value()) {
    return false;
  }

  std::optional<ProductSpecificationsSet> existing_set =
      product_specifications_service_->GetSetByUuid(
          product_group_for_page_->uuid);

  return existing_set.has_value() &&
         existing_set->url_infos().size() < kMaxTableSize &&
         (base::FeatureList::IsEnabled(commerce::kProductSpecifications) &&
                  base::FeatureList::IsEnabled(
                      commerce::kCompareConfirmationToast)
              ? !is_in_recommended_set_
              : true);
}

bool ProductSpecificationsPageActionController::WantsExpandedUi() {
  return product_group_for_page_.has_value();
}

void ProductSpecificationsPageActionController::ResetForNewNavigation(
    const GURL& url) {
  if (!shopping_service_ || !shopping_service_->GetAccountChecker() ||
      !commerce::CanFetchProductSpecificationsData(
          shopping_service_->GetAccountChecker())) {
    return;
  }
  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  current_url_ = url;
  got_product_response_for_page_ = false;
  product_group_for_page_ = std::nullopt;
  product_info_for_page_ = std::nullopt;
  // Initiate an update for the icon on navigation since we may not have product
  // info.
  NotifyHost();

  shopping_service_->GetProductInfoForUrl(
      url,
      base::BindOnce(
          &ProductSpecificationsPageActionController::HandleProductInfoResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsPageActionController::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& product_specifications_set) {
  auto& urls = product_specifications_set.urls();
  if (std::find(urls.begin(), urls.end(), current_url_) != urls.end()) {
    product_group_for_page_ = std::nullopt;
    is_in_recommended_set_ = false;
    NotifyHost();
  }
}

void ProductSpecificationsPageActionController::
    OnProductSpecificationsSetUpdate(
        const ProductSpecificationsSet& before_set,
        const ProductSpecificationsSet& after_set) {
  if (!product_group_for_page_.has_value() ||
      product_group_for_page_->uuid != after_set.uuid()) {
    return;
  }
  bool is_in_set = base::Contains(after_set.urls(), current_url_);
  if (is_in_set != is_in_recommended_set_ ||
      after_set.url_infos().size() >= kMaxTableSize) {
    is_in_recommended_set_ = is_in_set;
    NotifyHost();
  }
}

void ProductSpecificationsPageActionController::
    OnProductSpecificationsSetRemoved(const ProductSpecificationsSet& set) {
  if (product_group_for_page_.has_value() &&
      product_group_for_page_->uuid == set.uuid()) {
    product_group_for_page_ = std::nullopt;
    is_in_recommended_set_ = false;
    NotifyHost();
  }
}

void ProductSpecificationsPageActionController::OnIconClicked() {
  CHECK(product_group_for_page_.has_value());
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService() ||
      !product_group_for_page_.has_value()) {
    return;
  }
  std::optional<ProductSpecificationsSet> product_specifications_set =
      product_specifications_service_->GetSetByUuid(
          product_group_for_page_->uuid);
  if (!product_specifications_set.has_value()) {
    return;
  }
  std::vector<UrlInfo> existing_url_infos =
      product_specifications_set->url_infos();
  if (!is_in_recommended_set_) {
    existing_url_infos.emplace_back(current_url_, std::u16string());
    is_in_recommended_set_ = true;
  } else {
    GURL current_url = current_url_;
    auto it = base::ranges::find_if(
        existing_url_infos, [&current_url](const UrlInfo& query_url_info) {
          return query_url_info.url == current_url;
        });

    if (it != existing_url_infos.end()) {
      existing_url_infos.erase(it);
    }
    is_in_recommended_set_ = false;
  }
  product_specifications_service_->SetUrls(product_group_for_page_->uuid,
                                           std::move(existing_url_infos));

  NotifyHost();
}

bool ProductSpecificationsPageActionController::IsInRecommendedSet() {
  return is_in_recommended_set_;
}

std::u16string
ProductSpecificationsPageActionController::GetProductSpecificationsLabel(
    bool is_added) {
  if (!product_group_for_page_.has_value() ||
      product_group_for_page_->name.size() > kPageActionTitleMaxLength) {
    return is_added
               ? l10n_util::GetStringUTF16(
                     IDS_COMPARE_PAGE_ACTION_ADDED_DEFAULT)
               : l10n_util::GetStringUTF16(IDS_COMPARE_PAGE_ACTION_ADD_DEFAULT);
  }
  std::u16string set_name = base::UTF8ToUTF16(product_group_for_page_->name);
  return is_added ? l10n_util::GetStringFUTF16(IDS_COMPARE_PAGE_ACTION_ADDED,
                                               set_name)
                  : l10n_util::GetStringFUTF16(IDS_COMPARE_PAGE_ACTION_ADD,
                                               set_name);
}

std::u16string
ProductSpecificationsPageActionController::GetComparisonSetName() {
  CHECK(product_group_for_page_.has_value());
  return base::UTF8ToUTF16(product_group_for_page_->name);
}

GURL ProductSpecificationsPageActionController::GetComparisonTableURL() {
  CHECK(product_group_for_page_.has_value());
  return commerce::GetProductSpecsTabUrlForID(
      product_group_for_page_.value().uuid);
}

void ProductSpecificationsPageActionController::HandleProductInfoResponse(
    const GURL& url,
    const std::optional<const ProductInfo>& info) {
  if (url == current_url_ && info.has_value() &&
      shopping_service_->GetClusterManager()) {
    product_info_for_page_ = info;
    product_group_for_page_ = shopping_service_->GetClusterManager()
                                  ->GetProductGroupForCandidateProduct(url);
  }
  got_product_response_for_page_ = true;
  NotifyHost();
}
}  // namespace commerce
