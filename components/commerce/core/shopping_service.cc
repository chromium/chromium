// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace commerce {

ProductInfo::ProductInfo() = default;
ProductInfo::ProductInfo(const ProductInfo&) = default;
ProductInfo& ProductInfo::operator=(const ProductInfo&) = default;
ProductInfo::~ProductInfo() = default;
MerchantInfo::MerchantInfo() = default;
MerchantInfo::MerchantInfo(MerchantInfo&&) = default;
MerchantInfo::~MerchantInfo() = default;

ShoppingService::ShoppingService(
    bookmarks::BookmarkModel* bookmark_model,
    optimization_guide::NewOptimizationGuideDecider* opt_guide,
    PrefService* pref_service)
    : opt_guide_(opt_guide),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  // Register for the types of information we're allowed to receive from
  // optimization guide.
  if (opt_guide_) {
    std::vector<optimization_guide::proto::OptimizationType> types;

    // Don't register for info unless we're allowed to by an experiment.
    if (IsProductInfoApiEnabled() || IsPDPMetricsRecordingEnabled()) {
      types.push_back(
          optimization_guide::proto::OptimizationType::PRICE_TRACKING);
    }
    if (IsMerchantInfoApiEnabled()) {
      types.push_back(optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2);
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

void ShoppingService::WebWrapperCreated(WebWrapper* web) {}

void ShoppingService::DidNavigatePrimaryMainFrame(WebWrapper* web) {
  if (opt_guide_ &&
      (IsProductInfoApiEnabled() || IsPDPMetricsRecordingEnabled())) {
    UpdateProductInfoCacheForInsertion(web->GetLastCommittedURL());

    opt_guide_->CanApplyOptimization(
        web->GetLastCommittedURL(),
        optimization_guide::proto::OptimizationType::PRICE_TRACKING,
        base::BindOnce(
            [](base::WeakPtr<ShoppingService> service, const GURL& url,
               bool is_off_the_record,
               optimization_guide::OptimizationGuideDecision decision,
               const optimization_guide::OptimizationMetadata& metadata) {
              service->HandleOptGuideProductInfoResponse(
                  url,
                  base::BindOnce(
                      [](const GURL&, const absl::optional<ProductInfo>&) {}),
                  decision, metadata);

              service->PDPMetricsCallback(is_off_the_record, decision,
                                          metadata);
            },
            weak_ptr_factory_.GetWeakPtr(), web->GetLastCommittedURL(),
            web->IsOffTheRecord()));
  }
}

void ShoppingService::DidNavigateAway(WebWrapper* web, const GURL& from_url) {
  UpdateProductInfoCacheForRemoval(from_url);
}

void ShoppingService::DidFinishLoad(WebWrapper* web) {}

void ShoppingService::WebWrapperDestroyed(WebWrapper* web) {
  UpdateProductInfoCacheForRemoval(web->GetLastCommittedURL());
}

void ShoppingService::UpdateProductInfoCacheForInsertion(const GURL& url) {
  if (!IsProductInfoApiEnabled())
    return;

  auto it = product_info_cache_.find(url.spec());
  if (it != product_info_cache_.end()) {
    std::get<0>(it->second) = std::get<0>(it->second) + 1;
  } else {
    product_info_cache_[url.spec()] = std::make_tuple(1, false, nullptr);
    std::get<2>(product_info_cache_[url.spec()]).reset();
  }
}

void ShoppingService::UpdateProductInfoCache(
    const GURL& url,
    bool needs_js,
    std::unique_ptr<ProductInfo> info) {
  auto it = product_info_cache_.find(url.spec());
  if (it == product_info_cache_.end())
    return;

  int count = std::get<0>(it->second);
  product_info_cache_[url.spec()] =
      std::make_tuple(count, needs_js, std::move(info));
}

const ProductInfo* ShoppingService::GetFromProductInfoCache(const GURL& url) {
  auto it = product_info_cache_.find(url.spec());
  if (it == product_info_cache_.end())
    return nullptr;

  return std::get<2>(it->second).get();
}

void ShoppingService::UpdateProductInfoCacheForRemoval(const GURL& url) {
  if (!IsProductInfoApiEnabled())
    return;

  // Check if the previously navigated URL cache needs to be cleared. If more
  // than one tab was open with the same URL, keep it in the cache but decrement
  // the internal count.
  auto it = product_info_cache_.find(url.spec());
  if (it != product_info_cache_.end()) {
    if (std::get<0>(it->second) <= 1) {
      product_info_cache_.erase(it);
    } else {
      std::get<0>(it->second) = std::get<0>(it->second) - 1;
    }
  }
}

void ShoppingService::PDPMetricsCallback(
    bool is_off_the_record,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!IsPDPMetricsRecordingEnabled())
    return;

  metrics::RecordPDPStateForNavigation(decision, metadata, pref_service_,
                                       is_off_the_record);
}

void ShoppingService::GetProductInfoForUrl(const GURL& url,
                                           ProductInfoCallback callback) {
  if (!opt_guide_)
    return;

  // Crash if this API is used without a valid experiment.
  CHECK(IsProductInfoApiEnabled());

  const ProductInfo* cachedInfo = GetFromProductInfoCache(url);
  if (cachedInfo) {
    absl::optional<ProductInfo> info;
    // Make a copy based on the cached value.
    info.emplace(*cachedInfo);
    std::move(callback).Run(url, info);
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingService::HandleOptGuideProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ShoppingService::GetMerchantInfoForUrl(const GURL& url,
                                            MerchantInfoCallback callback) {
  if (!opt_guide_)
    return;

  // Crash if this API is used without a valid experiment.
  CHECK(IsMerchantInfoApiEnabled());

  opt_guide_->CanApplyOptimization(
      url,
      optimization_guide::proto::OptimizationType::MERCHANT_TRUST_SIGNALS_V2,
      base::BindOnce(&ShoppingService::HandleOptGuideMerchantInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

bool ShoppingService::IsProductInfoApiEnabled() {
  return base::FeatureList::IsEnabled(kShoppingList);
}

bool ShoppingService::IsPDPMetricsRecordingEnabled() {
  return base::FeatureList::IsEnabled(commerce::kShoppingPDPMetrics);
}

bool ShoppingService::IsMerchantInfoApiEnabled() {
  return base::FeatureList::IsEnabled(kCommerceMerchantViewer);
}

void ShoppingService::HandleOptGuideProductInfoResponse(
    const GURL& url,
    ProductInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!IsProductInfoApiEnabled())
    return;

  // If optimization guide returns negative, return a negative signal with an
  // empty data object.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  std::unique_ptr<ProductInfo> info = nullptr;

  if (metadata.any_metadata().has_value()) {
    absl::optional<commerce::PriceTrackingData> parsed_any =
        optimization_guide::ParsedAnyMetadata<commerce::PriceTrackingData>(
            metadata.any_metadata().value());
    commerce::PriceTrackingData price_data = parsed_any.value();
    if (parsed_any.has_value() && price_data.IsInitialized()) {
      commerce::BuyableProduct buyable_product = price_data.buyable_product();

      info = std::make_unique<ProductInfo>();

      if (buyable_product.has_title())
        info->title = buyable_product.title();

      if (buyable_product.has_image_url() &&
          base::FeatureList::IsEnabled(kCommerceAllowServerImages)) {
        info->image_url = GURL(buyable_product.image_url());
      }

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

  absl::optional<ProductInfo> optional_info;
  if (info) {
    optional_info.emplace(*info);
    UpdateProductInfoCache(url, true, std::move(info));
  }

  // TODO(mdjones): Longer-term it probably makes sense to wait until the
  // javascript has run to execute this.
  std::move(callback).Run(url, optional_info);
}

void ShoppingService::HandleOptGuideMerchantInfoResponse(
    const GURL& url,
    MerchantInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If optimization guide returns negative, return a negative signal with an
  // empty data object.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  absl::optional<MerchantInfo> info;

  if (metadata.any_metadata().has_value()) {
    absl::optional<commerce::MerchantTrustSignalsV2> parsed_any =
        optimization_guide::ParsedAnyMetadata<commerce::MerchantTrustSignalsV2>(
            metadata.any_metadata().value());
    commerce::MerchantTrustSignalsV2 merchant_data = parsed_any.value();
    if (parsed_any.has_value() && merchant_data.IsInitialized()) {
      info.emplace();

      if (merchant_data.has_merchant_star_rating()) {
        info->star_rating = merchant_data.merchant_star_rating();
      }

      if (merchant_data.has_merchant_count_rating()) {
        info->count_rating = merchant_data.merchant_count_rating();
      }

      if (merchant_data.has_merchant_details_page_url()) {
        info->details_page_url =
            GURL(merchant_data.merchant_details_page_url());
      }

      if (merchant_data.has_has_return_policy()) {
        info->has_return_policy = merchant_data.has_return_policy();
      }

      if (merchant_data.has_non_personalized_familiarity_score()) {
        info->non_personalized_familiarity_score =
            merchant_data.non_personalized_familiarity_score();
      }

      if (merchant_data.has_contains_sensitive_content()) {
        info->contains_sensitive_content =
            merchant_data.contains_sensitive_content();
      }

      if (merchant_data.has_proactive_message_disabled()) {
        info->proactive_message_disabled =
            merchant_data.proactive_message_disabled();
      }
    }
  }

  std::move(callback).Run(url, std::move(info));
}

void ShoppingService::Shutdown() {}

ShoppingService::~ShoppingService() = default;

}  // namespace commerce
