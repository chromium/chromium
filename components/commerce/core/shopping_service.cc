// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/grit/components_resources.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "ui/base/resource/resource_bundle.h"

namespace commerce {

const char kOgTitle[] = "title";
const char kOgImage[] = "image";
const char kOgPriceCurrency[] = "price:currency";
const char kOgPriceAmount[] = "price:amount";
const long kToMicroCurrency = 1e6;

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
      types.push_back(optimization_guide::proto::OptimizationType::
                          MERCHANT_TRUST_SIGNALS_V2);
    }

    opt_guide_->RegisterOptimizationTypes(types);
  }

  if (bookmark_model) {
    shopping_bookmark_observer_ =
        std::make_unique<ShoppingBookmarkModelObserver>(bookmark_model);
  }

  subscriptions_manager_ = std::make_unique<SubscriptionsManager>();
}

void ShoppingService::RegisterPrefs(PrefRegistrySimple* registry) {
  // This pref value is queried from server. Set initial value as true so our
  // features can be correctly set up while waiting for the server response.
  registry->RegisterBooleanPref(commerce::kWebAndAppActivityEnabledForShopping,
                                true);
}

void ShoppingService::WebWrapperCreated(WebWrapper* web) {}

void ShoppingService::DidNavigatePrimaryMainFrame(WebWrapper* web) {
  HandleDidNavigatePrimaryMainFrameForProductInfo(web);
}

void ShoppingService::HandleDidNavigatePrimaryMainFrameForProductInfo(
    WebWrapper* web) {
  // We need optimization guide and one of the features that depend on the
  // price tracking signal to be enabled to do any of this.
  if (!opt_guide_ ||
      (!IsProductInfoApiEnabled() && !IsPDPMetricsRecordingEnabled())) {
    return;
  }

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

            service->PDPMetricsCallback(is_off_the_record, decision, metadata);
          },
          weak_ptr_factory_.GetWeakPtr(), web->GetLastCommittedURL(),
          web->IsOffTheRecord()));
}

void ShoppingService::DidNavigateAway(WebWrapper* web, const GURL& from_url) {
  UpdateProductInfoCacheForRemoval(from_url);
}

void ShoppingService::DidFinishLoad(WebWrapper* web) {
  HandleDidFinishLoadForProductInfo(web);
}

void ShoppingService::HandleDidFinishLoadForProductInfo(WebWrapper* web) {
  if (!IsProductInfoApiEnabled())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = product_info_cache_.find(web->GetLastCommittedURL().spec());
  // If there is both an entry in the cache and the javascript fallback needs
  // to run, run it.
  if (it != product_info_cache_.end() && std::get<1>(it->second)) {
    // Since we're about to run the JS, flip the flag in the cache.
    std::get<1>(it->second) = false;

    std::string script =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_QUERY_SHOPPING_META_JS);

    web->RunJavascript(
        base::UTF8ToUTF16(script),
        base::BindOnce(&ShoppingService::OnProductInfoJavascriptResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       GURL(web->GetLastCommittedURL())));
  }
}

void ShoppingService::OnProductInfoJavascriptResult(const GURL url,
                                                    base::Value result) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      result.GetString(),
      base::BindOnce(&ShoppingService::OnProductInfoJsonSanitizationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void ShoppingService::OnProductInfoJsonSanitizationCompleted(
    const GURL url,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value())
    return;

  auto it = product_info_cache_.find(url.spec());
  // If there was no entry or the data doesn't need javascript run, do
  // nothing.
  if (it != product_info_cache_.end())
    MergeProductInfoData(std::get<2>(it->second).get(), result.value());
}

void ShoppingService::WebWrapperDestroyed(WebWrapper* web) {
  UpdateProductInfoCacheForRemoval(web->GetLastCommittedURL());
}

void ShoppingService::UpdateProductInfoCacheForInsertion(const GURL& url) {
  if (!IsProductInfoApiEnabled())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  const ProductInfo* cached_info = GetFromProductInfoCache(url);
  if (cached_info) {
    absl::optional<ProductInfo> info;
    // Make a copy based on the cached value.
    info.emplace(*cached_info);
    std::move(callback).Run(url, info);
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingService::HandleOptGuideProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

absl::optional<ProductInfo> ShoppingService::GetAvailableProductInfoForUrl(
    const GURL& url) {
  absl::optional<ProductInfo> optional_info;

  const ProductInfo* cached_info = GetFromProductInfoCache(url);
  if (cached_info)
    optional_info.emplace(*cached_info);

  return optional_info;
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

void MergeProductInfoData(ProductInfo* info, base::Value& on_page_data_map) {
  if (!info)
    return;

  // This will be true if any of the data found in |on_page_data_map| is used to
  // populate fields in |meta|.
  bool data_was_merged = false;

  for (auto it : on_page_data_map.DictItems()) {
    if (base::CompareCaseInsensitiveASCII(it.first, kOgTitle) == 0) {
      if (info->title.empty()) {
        info->title = it.second.GetString();
        base::UmaHistogramEnumeration(
            "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
            ProductInfoFallback::kTitle, ProductInfoFallback::kMaxValue);
        data_was_merged = true;
      }
    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgImage) == 0) {
      // If the product already has an image, add the one found on the page as
      // a fallback. The original image, if it exists, should have been
      // retrieved from the proto received from optimization guide before this
      // callback runs.
      if (info->image_url.is_empty()) {
        GURL og_url(it.second.GetString());
        info->image_url.Swap(&og_url);
        base::UmaHistogramEnumeration(
            "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
            ProductInfoFallback::kLeadImage, ProductInfoFallback::kMaxValue);
        data_was_merged = true;
      }
      // TODO(mdjones): Add capacity for fallback images when necessary.

    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgPriceCurrency) ==
               0) {
      if (info->amount_micros <= 0) {
        double amount = 0;
        if (base::StringToDouble(
                *on_page_data_map.FindStringKey(kOgPriceAmount), &amount)) {
          // Currency is stored in micro-units rather than standard units, so we
          // need to convert (open graph provides standard units).
          info->amount_micros = amount * kToMicroCurrency;
          info->currency_code = it.second.GetString();
          base::UmaHistogramEnumeration(
              "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
              ProductInfoFallback::kPrice, ProductInfoFallback::kMaxValue);
          data_was_merged = true;
        }
      }
    }
  }

  base::UmaHistogramBoolean(
      "Commerce.ShoppingService.ProductInfo.FallbackDataUsed", data_was_merged);
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
        GURL details_page_url = GURL(merchant_data.merchant_details_page_url());
        if (details_page_url.is_valid()) {
          info->details_page_url = details_page_url;
        }
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

void ShoppingService::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  subscriptions_manager_->Subscribe(std::move(subscriptions),
                                    std::move(callback));
}

void ShoppingService::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  subscriptions_manager_->Unsubscribe(std::move(subscriptions),
                                      std::move(callback));
}

void ShoppingService::Shutdown() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ShoppingService::~ShoppingService() = default;

}  // namespace commerce
