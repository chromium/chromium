// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/escape.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace commerce {

GURL GetProductSpecsTabUrl(const std::vector<GURL>& urls) {
  auto urls_list = base::Value::List();

  for (auto& url : urls) {
    urls_list.Append(url.spec());
  }

  std::string json;
  if (!base::JSONWriter::Write(urls_list, &json)) {
    return GURL(commerce::kChromeUICompareUrl);
  }

  return net::AppendQueryParameter(GURL(commerce::kChromeUICompareUrl), "urls",
                                   json);
}

GURL GetProductSpecsTabUrlForID(const base::Uuid& uuid) {
  return net::AppendQueryParameter(GURL(commerce::kChromeUICompareUrl), "id",
                                   uuid.AsLowercaseString());
}

std::unique_ptr<ProductInfo> OptGuideResultToProductInfo(
    const optimization_guide::OptimizationMetadata& metadata,
    bool can_load_product_specs_full_page_ui) {
  if (!metadata.any_metadata().has_value()) {
    return nullptr;
  }

  std::optional<commerce::PriceTrackingData> parsed_any =
      optimization_guide::ParsedAnyMetadata<commerce::PriceTrackingData>(
          metadata.any_metadata().value());
  commerce::PriceTrackingData price_data = parsed_any.value();

  if (!parsed_any.has_value() || !price_data.IsInitialized()) {
    return nullptr;
  }

  const commerce::BuyableProduct buyable_product = price_data.buyable_product();

  std::unique_ptr<ProductInfo> info = std::make_unique<ProductInfo>();

  if (buyable_product.has_title()) {
    info->title = buyable_product.title();
  }

  if (buyable_product.has_gpc_title()) {
    info->product_cluster_title = buyable_product.gpc_title();
  }

  if (buyable_product.has_image_url()) {
    info->server_image_available = true;
    info->image_url = GURL(buyable_product.image_url());
  } else {
    info->server_image_available = false;
  }

  if (buyable_product.has_offer_id()) {
    info->offer_id = buyable_product.offer_id();
  }

  if (buyable_product.has_product_cluster_id()) {
    info->product_cluster_id = buyable_product.product_cluster_id();
  }

  if (buyable_product.has_current_price()) {
    info->currency_code = buyable_product.current_price().currency_code();
    info->amount_micros = buyable_product.current_price().amount_micros();
  }

  if (buyable_product.has_country_code()) {
    info->country_code = buyable_product.country_code();
  }

  // Check to see if there was a price drop associated with this product. Those
  // prices take priority over what BuyableProduct has.
  if (price_data.has_product_update()) {
    const commerce::ProductPriceUpdate price_update =
        price_data.product_update();

    // Both new and old price should exist and have the same currency code.
    bool currency_codes_match = price_update.new_price().currency_code() ==
                                price_update.old_price().currency_code();

    if (price_update.has_new_price() &&
        info->currency_code == price_update.new_price().currency_code() &&
        currency_codes_match) {
      info->amount_micros = price_update.new_price().amount_micros();
    }
    if (price_update.has_old_price() &&
        info->currency_code == price_update.old_price().currency_code() &&
        currency_codes_match) {
      info->previous_amount_micros.emplace(
          price_update.old_price().amount_micros());
    }
  }

  if (buyable_product.has_category_data()) {
    info->category_data = buyable_product.category_data();
  }

  // TODO(376128060): Remove the feature check after M132.
  if (can_load_product_specs_full_page_ui) {
    for (int i = 0; i < buyable_product.price_summary_size(); ++i) {
      info->price_summary.push_back(buyable_product.price_summary(i));
    }

    if (buyable_product.has_price_display_recommendation()) {
      info->price_display_recommendation =
          buyable_product.price_display_recommendation();
    }
  }

  return info;
}

void MaybeUseAlternateShoppingServer(
    endpoint_fetcher::EndpointFetcher::RequestParams::Builder& params_builder) {
  if (base::FeatureList::IsEnabled(commerce::kShoppingAlternateServer)) {
    params_builder.SetHeaders(
        std::vector<endpoint_fetcher::EndpointFetcher::RequestParams::Header>{
            {kAlternateServerHeaderName, kAlternateServerHeaderTrueValue}});
  }
}

}  // namespace commerce
