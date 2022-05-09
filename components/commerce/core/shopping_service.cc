// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace commerce {

ProductInfo::ProductInfo() = default;
ProductInfo::~ProductInfo() = default;

ShoppingService::ShoppingService(
    bookmarks::BookmarkModel* bookmark_model,
    optimization_guide::NewOptimizationGuideDecider* opt_guide)
    : opt_guide_(opt_guide), weak_ptr_factory_(this) {
  // Register for the types of information we're allowed to receive from
  // optimization guide.
  if (opt_guide_) {
    std::vector<optimization_guide::proto::OptimizationType> types;

    // Don't register for info unless we're allowed to by an experiment.
    if (IsProductInfoApiEnabled()) {
      types.push_back(
          optimization_guide::proto::OptimizationType::PRICE_TRACKING);
    }

    opt_guide_->RegisterOptimizationTypes(types);
  }

  if (bookmark_model) {
    shopping_bookmark_observer_ =
        std::make_unique<ShoppingBookmarkModelObserver>(bookmark_model);
  }
}

void ShoppingService::RegisterPrefs(PrefRegistrySimple* registry) {
  // This pref value is queried from server. Set initial value as true so our
  // features can be correctly set up while waiting for the server response.
  registry->RegisterBooleanPref(commerce::kWebAndAppActivityEnabledForShopping,
                                true);
}

void ShoppingService::GetProductInfoForUrl(const GURL& url,
                                           ProductInfoCallback callback) {
  if (!opt_guide_)
    return;

  // Crash if this API is used without a valid experiment.
  CHECK(IsProductInfoApiEnabled());

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingService::HandleOptGuideProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

bool ShoppingService::IsProductInfoApiEnabled() {
  return base::FeatureList::IsEnabled(kShoppingList);
}

void ShoppingService::HandleOptGuideProductInfoResponse(
    const GURL& url,
    ProductInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If optimization guide returns negative, return a negative signal with an
  // empty data object.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  absl::optional<ProductInfo> info;

  if (metadata.any_metadata().has_value()) {
    absl::optional<commerce::PriceTrackingData> parsed_any =
        optimization_guide::ParsedAnyMetadata<commerce::PriceTrackingData>(
            metadata.any_metadata().value());
    commerce::PriceTrackingData price_data = parsed_any.value();
    if (parsed_any.has_value() && price_data.IsInitialized()) {
      commerce::BuyableProduct buyable_product = price_data.buyable_product();

      info.emplace();

      if (buyable_product.has_title())
        info->title = buyable_product.title();

      if (buyable_product.has_image_url())
        info->image_url = GURL(buyable_product.image_url());

      if (buyable_product.has_offer_id())
        info->offer_id = buyable_product.offer_id();

      if (buyable_product.has_product_cluster_id())
        info->product_cluster_id = buyable_product.product_cluster_id();

      if (buyable_product.has_current_price()) {
        info->currency_code = buyable_product.current_price().currency_code();
        info->amount_micros = buyable_product.current_price().amount_micros();
      }

      if (buyable_product.has_country_code())
        info->country_code = buyable_product.country_code();
    }
  }

  std::move(callback).Run(url, info);
}

void ShoppingService::Shutdown() {}

ShoppingService::~ShoppingService() = default;

}  // namespace commerce
