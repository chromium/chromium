// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_service_handler.h"

#include <memory>
#include <vector>

#include "base/check_is_test.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/webui/webui_utils.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {
namespace {

shopping_service::mojom::UrlInfoPtr UrlInfoToMojo(const UrlInfo& url_info) {
  auto url_info_ptr = shopping_service::mojom::UrlInfo::New();
  url_info_ptr->url = url_info.url;
  url_info_ptr->title = base::UTF16ToUTF8(url_info.title);
  url_info_ptr->previewText = url_info.previewText.value_or("");
  url_info_ptr->favicon_url = url_info.favicon_url.value_or(GURL());
  return url_info_ptr;
}

std::vector<shopping_service::mojom::UrlInfoPtr> UrlInfoListToMojo(
    const std::vector<UrlInfo>& url_infos) {
  std::vector<shopping_service::mojom::UrlInfoPtr> url_info_ptr_list;

  for (const UrlInfo& url_info : url_infos) {
    url_info_ptr_list.push_back(UrlInfoToMojo(url_info));
  }
  return url_info_ptr_list;
}

shopping_service::mojom::PriceInsightsInfoPtr PriceInsightsInfoToMojoObject(
    const std::optional<PriceInsightsInfo>& info,
    const std::string& locale) {
  auto insights_info = shopping_service::mojom::PriceInsightsInfo::New();

  if (!info.has_value()) {
    return insights_info;
  }

  if (info->product_cluster_id.has_value()) {
    insights_info->cluster_id = info->product_cluster_id.value();
  } else {
    CHECK_IS_TEST();
  }

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(info->currency_code,
                                                    locale);
  formatter->SetMaxFractionalDigits(2);

  if (info->typical_low_price_micros.has_value() &&
      info->typical_high_price_micros.has_value()) {
    insights_info->typical_low_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_low_price_micros.value()) /
            kToMicroCurrency)));

    insights_info->typical_high_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_high_price_micros.value()) /
            kToMicroCurrency)));
  }

  if (info->catalog_attributes.has_value()) {
    insights_info->catalog_attributes = info->catalog_attributes.value();
  }

  if (info->jackpot_url.has_value()) {
    insights_info->jackpot = info->jackpot_url.value();
  }

  shopping_service::mojom::PriceInsightsInfo::PriceBucket bucket;
  switch (info->price_bucket) {
    case PriceBucket::kUnknown:
      bucket =
          shopping_service::mojom::PriceInsightsInfo::PriceBucket::kUnknown;
      break;
    case PriceBucket::kLowPrice:
      bucket = shopping_service::mojom::PriceInsightsInfo::PriceBucket::kLow;
      break;
    case PriceBucket::kTypicalPrice:
      bucket =
          shopping_service::mojom::PriceInsightsInfo::PriceBucket::kTypical;
      break;
    case PriceBucket::kHighPrice:
      bucket = shopping_service::mojom::PriceInsightsInfo::PriceBucket::kHigh;
      break;
  }
  insights_info->bucket = bucket;

  insights_info->has_multiple_catalogs = info->has_multiple_catalogs;

  for (const auto& history_price : info->catalog_history_prices) {
    auto point = shopping_service::mojom::PricePoint::New();
    point->date = std::get<0>(history_price);

    auto price =
        static_cast<float>(std::get<1>(history_price)) / kToMicroCurrency;
    point->price = price;
    point->formatted_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(price)));
    insights_info->history.push_back(std::move(point));
  }

  insights_info->locale = locale;
  insights_info->currency_code = info->currency_code;

  return insights_info;
}

}  // namespace

using shared::mojom::BookmarkProductInfo;
using shared::mojom::BookmarkProductInfoPtr;

ShoppingServiceHandler::ShoppingServiceHandler(
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver,
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    std::unique_ptr<Delegate> delegate,
    optimization_guide::ModelQualityLogsUploaderService*
        model_quality_logs_uploader_service)
    : receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      pref_service_(prefs),
      tracker_(tracker),
      delegate_(std::move(delegate)),
      model_quality_logs_uploader_service_(
          model_quality_logs_uploader_service) {
  // It is safe to schedule updates. If the feature is disabled, no new
  // information will be fetched or provided to the frontend.
  shopping_service_->ScheduleSavedProductUpdate();

  if (shopping_service_->GetAccountChecker()) {
    locale_ = shopping_service_->GetAccountChecker()->GetLocale();
  }
}

ShoppingServiceHandler::~ShoppingServiceHandler() = default;

void ShoppingServiceHandler::GetProductInfoForCurrentUrl(
    GetProductInfoForCurrentUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker()) ||
      !delegate_ || !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shared::mojom::ProductInfo::New());
    return;
  }

  shopping_service_->GetProductInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             GetProductInfoForCurrentUrlCallback callback, const GURL& url,
             const std::optional<const ProductInfo>& info) {
            if (!handler) {
              std::move(callback).Run(shared::mojom::ProductInfo::New());
              return;
            }

            std::move(callback).Run(
                ProductInfoToMojoProduct(url, info, handler->locale_));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetProductInfoForUrl(
    const GURL& url,
    GetProductInfoForUrlCallback callback) {
  shopping_service_->GetProductInfoForUrl(
      url, base::BindOnce(
               [](base::WeakPtr<ShoppingServiceHandler> handler,
                  GetProductInfoForUrlCallback callback, const GURL& url,
                  const std::optional<const ProductInfo>& info) {
                 if (!handler) {
                   std::move(callback).Run(url,
                                           shared::mojom::ProductInfo::New());
                   return;
                 }

                 std::move(callback).Run(url, ProductInfoToMojoProduct(
                                                  url, info, handler->locale_));
               },
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetProductInfoForUrls(
    const std::vector<GURL>& urls,
    GetProductInfoForUrlsCallback callback) {
  shopping_service_->GetProductInfoForUrls(
      urls,
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             std::vector<GURL> urls, GetProductInfoForUrlsCallback callback,
             const std::map<GURL, std::optional<ProductInfo>> info_map) {
            std::vector<shared::mojom::ProductInfoPtr> info_list;
            // Provide the URLs/info in the same order they were requested.
            for (GURL& url : urls) {
              info_list.push_back(ProductInfoToMojoProduct(
                  url, info_map.at(url), handler->locale_));
            }
            std::move(callback).Run(std::move(info_list));
          },
          weak_ptr_factory_.GetWeakPtr(), urls, std::move(callback)));
}

void ShoppingServiceHandler::IsShoppingListEligible(
    IsShoppingListEligibleCallback callback) {
  std::move(callback).Run(shopping_service_->IsShoppingListEligible());
}

void ShoppingServiceHandler::GetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback) {
  // The URL may or may not have a bookmark associated with it. Prioritize
  // accessing the product info for the URL before looking at an existing
  // bookmark.
  shopping_service_->GetProductInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             GetPriceTrackingStatusForCurrentUrlCallback callback,
             const GURL& url, const std::optional<const ProductInfo>& info) {
            if (!info.has_value() || !info->product_cluster_id.has_value() ||
                !handler || !CanTrackPrice(info)) {
              std::move(callback).Run(false);
              return;
            }
            CommerceSubscription sub(
                SubscriptionType::kPriceTrack,
                IdentifierType::kProductClusterId,
                base::NumberToString(info->product_cluster_id.value()),
                ManagementType::kUserManaged);

            handler->shopping_service_->IsSubscribed(
                sub,
                base::BindOnce(
                    [](GetPriceTrackingStatusForCurrentUrlCallback callback,
                       bool subscribed) {
                      std::move(callback).Run(subscribed);
                    },
                    std::move(callback)));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker()) ||
      !delegate_ || !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shopping_service::mojom::PriceInsightsInfo::New());
    return;
  }

  shopping_service_->GetPriceInsightsInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          &ShoppingServiceHandler::OnFetchPriceInsightsInfoForCurrentUrl,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetPriceInsightsInfoForUrl(
    const GURL& url,
    GetPriceInsightsInfoForUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker())) {
    std::move(callback).Run(url,
                            shopping_service::mojom::PriceInsightsInfo::New());
    return;
  }

  shopping_service_->GetPriceInsightsInfoForUrl(
      url, base::BindOnce(
               [](base::WeakPtr<ShoppingServiceHandler> handler,
                  GetPriceInsightsInfoForUrlCallback callback, const GURL& url,
                  const std::optional<PriceInsightsInfo>& info) {
                 if (!handler) {
                   return;
                 }

                 std::move(callback).Run(url, PriceInsightsInfoToMojoObject(
                                                  info, handler->locale_));
               },
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetUrlInfosForProductTabs(
    GetUrlInfosForProductTabsCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run({});
    return;
  }

  shopping_service_->GetUrlInfosForWebWrappersWithProducts(base::BindOnce(
      [](GetUrlInfosForProductTabsCallback callback,
         const std::vector<UrlInfo> infos) {
        std::move(callback).Run(UrlInfoListToMojo(infos));
      },
      std::move(callback)));
}

void ShoppingServiceHandler::GetUrlInfosForRecentlyViewedTabs(
    GetUrlInfosForRecentlyViewedTabsCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(UrlInfoListToMojo(
      shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers()));
}

void ShoppingServiceHandler::OnFetchPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback,
    const GURL& url,
    const std::optional<PriceInsightsInfo>& info) {
  std::move(callback).Run(PriceInsightsInfoToMojoObject(info, locale_));
}

void ShoppingServiceHandler::OnGetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback,
    bool tracked) {
  std::move(callback).Run(tracked);
}

void ShoppingServiceHandler::OpenUrlInNewTab(const GURL& url) {
  if (delegate_) {
    delegate_->OpenUrlInNewTab(url);
  }
}

void ShoppingServiceHandler::SwitchToOrOpenTab(const GURL& url) {
  if (delegate_) {
    delegate_->SwitchToOrOpenTab(url);
  }
}

}  // namespace commerce
