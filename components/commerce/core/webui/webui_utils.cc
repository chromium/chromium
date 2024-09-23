// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/webui_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_types.h"
#include "components/payments/core/currency_formatter.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {

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
  return product_info;
}

}  // namespace commerce
