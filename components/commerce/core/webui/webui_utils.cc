// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/webui_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/payments/core/currency_formatter.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {

using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_PRICE_UNDETERMINED;
using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE;
using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_LOWER_BOUND;
using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_UPPER_BOUND;
using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_SINGLE_PRICE;
using BuyableProduct_PriceDisplayRecommendation::
    BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_UNSPECIFIED;

using PriceSummary_ProductOfferCondition::
    PriceSummary_ProductOfferCondition_CONDITION_ANY;

namespace {

std::string GetFormattedCurrencyFromMicros(
    uint64_t amount_micros,
    payments::CurrencyFormatter* formatter) {
  return base::UTF16ToUTF8(formatter->Format(base::NumberToString(
      static_cast<float>(amount_micros) / kToMicroCurrency)));
}

// Generate a string that displays the price summary in the format specified by
// the provided product info.
std::string GetFormattedPriceSummary(const ProductInfo& product_info,
                                     payments::CurrencyFormatter* formatter) {
  std::string current_price =
      GetFormattedCurrencyFromMicros(product_info.amount_micros, formatter);
  size_t index = 0;
  for (const auto& summary : product_info.price_summary) {
    if (summary.is_preferred()) {
      break;
    }
    index++;
  }

  if (index >= product_info.price_summary.size()) {
    return current_price;
  }

  const PriceSummary& summary = product_info.price_summary[index];

  std::string lowest_price = "";
  if (summary.has_lowest_price()) {
    lowest_price = GetFormattedCurrencyFromMicros(
        summary.lowest_price().amount_micros(), formatter);
  }

  std::string highest_price = "";
  if (summary.has_highest_price()) {
    highest_price = GetFormattedCurrencyFromMicros(
        summary.highest_price().amount_micros(), formatter);
  }

  switch (product_info.price_display_recommendation.value()) {
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE:
      if (!lowest_price.empty() && !highest_price.empty()) {
        return lowest_price + " - " + highest_price;
      }
      return current_price;
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_LOWER_BOUND:
      if (!lowest_price.empty()) {
        return lowest_price + "+";
      }
      return current_price;
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_UPPER_BOUND:
      if (!highest_price.empty()) {
        return highest_price;
      }
      return current_price;
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_PRICE_UNDETERMINED:
      return "-";
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_SINGLE_PRICE:
      return current_price;
    case BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_UNSPECIFIED:
      return "";
    default:
      break;
  }

  return current_price;
}

}  // namespace

shopping_service::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const std::optional<const ProductInfo>& info,
    const std::string& locale) {
  auto product_info = shopping_service::mojom::ProductInfo::New();

  if (!info.has_value()) {
    return product_info;
  }

  product_info->title = info->title;
  product_info->cluster_title = info->product_cluster_title;
  product_info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(url)));
  product_info->product_url = url;
  product_info->image_url = info->image_url;
  if (info->product_cluster_id.has_value()) {
    product_info->cluster_id = info->product_cluster_id.value();
  }

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(info->currency_code,
                                                    locale);
  formatter->SetMaxFractionalDigits(2);

  product_info->current_price =
      base::UTF16ToUTF8(formatter->Format(base::NumberToString(
          static_cast<float>(info->amount_micros) / kToMicroCurrency)));

  // Only send the previous price if it is higher than the current price.
  if (info->previous_amount_micros.has_value() &&
      info->previous_amount_micros.value() > info->amount_micros) {
    product_info->previous_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->previous_amount_micros.value()) /
            kToMicroCurrency)));
  }

  for (const auto& product_category :
       info->category_data.product_categories()) {
    std::string category_labels;
    bool is_first = true;
    for (const auto& category_label : product_category.category_labels()) {
      if (!is_first) {
        category_labels.append(">");
      }
      is_first = false;
      category_labels.append(category_label.category_short_label().empty()
                                 ? category_label.category_default_label()
                                 : category_label.category_short_label());
    }
    product_info->category_labels.push_back(std::move(category_labels));
  }

  if (info->price_summary.size() > 0) {
    product_info->price_summary =
        GetFormattedPriceSummary(info.value(), formatter.get());
  }

  return product_info;
}

shared::mojom::ProductSpecificationsSetPtr ProductSpecsSetToMojo(
    const ProductSpecificationsSet& set) {
  auto set_ptr = shared::mojom::ProductSpecificationsSet::New();

  set_ptr->name = set.name();
  set_ptr->uuid = set.uuid();

  for (const auto& url : set.urls()) {
    set_ptr->urls.push_back(url);
  }

  return set_ptr;
}

}  // namespace commerce
