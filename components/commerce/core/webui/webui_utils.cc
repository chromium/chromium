// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/webui_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
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

shared::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const std::optional<const ProductInfo>& info,
    const std::string& locale) {
  auto product_info = shared::mojom::ProductInfo::New();

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

shared::mojom::BookmarkProductInfoPtr BookmarkNodeToMojoProduct(
    bookmarks::BookmarkModel& model,
    const bookmarks::BookmarkNode* node,
    const std::string& locale) {
  auto bookmark_info = shared::mojom::BookmarkProductInfo::New();
  bookmark_info->bookmark_id = node->id();

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(&model, node);
  const power_bookmarks::ShoppingSpecifics specifics =
      meta->shopping_specifics();

  bookmark_info->info = shared::mojom::ProductInfo::New();
  bookmark_info->info->title = specifics.title();
  bookmark_info->info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(node->url())));

  bookmark_info->info->product_url = node->url();
  bookmark_info->info->image_url = GURL(meta->lead_image().url());
  bookmark_info->info->cluster_id = specifics.product_cluster_id();

  const power_bookmarks::ProductPrice price = specifics.current_price();
  std::string currency_code = price.currency_code();

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(currency_code, locale);
  formatter->SetMaxFractionalDigits(2);

  bookmark_info->info->current_price =
      base::UTF16ToUTF8(formatter->Format(base::NumberToString(
          static_cast<float>(price.amount_micros()) / kToMicroCurrency)));

  // Only send the previous price if it is higher than the current price. This
  // is exclusively used to decide whether to show the price drop chip in the
  // UI.
  if (specifics.has_previous_price() &&
      specifics.previous_price().amount_micros() >
          specifics.current_price().amount_micros()) {
    const power_bookmarks::ProductPrice previous_price =
        specifics.previous_price();
    bookmark_info->info->previous_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(previous_price.amount_micros()) /
            kToMicroCurrency)));
  }

  return bookmark_info;
}

}  // namespace commerce
