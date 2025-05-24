// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_types.h"

namespace commerce {

ProductInfo::ProductInfo() = default;
ProductInfo::ProductInfo(const ProductInfo&) = default;
ProductInfo& ProductInfo::operator=(const ProductInfo&) = default;
ProductInfo::~ProductInfo() = default;

ProductSpecifications::ProductSpecifications() = default;

ProductSpecifications::ProductSpecifications(const ProductSpecifications&) =
    default;
ProductSpecifications::~ProductSpecifications() = default;

ProductSpecifications::DescriptionText::DescriptionText() = default;
ProductSpecifications::DescriptionText::DescriptionText(
    const ProductSpecifications::DescriptionText&) = default;
ProductSpecifications::DescriptionText::~DescriptionText() = default;

ProductSpecifications::Description::Description() = default;
ProductSpecifications::Description::Description(
    const ProductSpecifications::Description&) = default;
ProductSpecifications::Description::~Description() = default;

ProductSpecifications::Description::Option::Option() = default;
ProductSpecifications::Description::Option::Option(
    const ProductSpecifications::Description::Option&) = default;
ProductSpecifications::Description::Option::~Option() = default;

ProductSpecifications::Product::Product() = default;
ProductSpecifications::Product::Product(const ProductSpecifications::Product&) =
    default;
ProductSpecifications::Product::~Product() = default;

ProductSpecifications::Value::Value() = default;
ProductSpecifications::Value::Value(const ProductSpecifications::Value&) =
    default;
ProductSpecifications::Value::~Value() = default;

MerchantInfo::MerchantInfo() = default;
MerchantInfo::MerchantInfo(const MerchantInfo&) = default;
MerchantInfo& MerchantInfo::operator=(const MerchantInfo&) = default;
MerchantInfo::MerchantInfo(MerchantInfo&&) = default;
MerchantInfo::~MerchantInfo() = default;

PriceInsightsInfo::PriceInsightsInfo() = default;
PriceInsightsInfo::PriceInsightsInfo(const PriceInsightsInfo&) = default;
PriceInsightsInfo& PriceInsightsInfo::operator=(const PriceInsightsInfo&) =
    default;
PriceInsightsInfo::~PriceInsightsInfo() = default;

DiscountInfo::DiscountInfo() = default;
DiscountInfo::DiscountInfo(const DiscountInfo&) = default;
DiscountInfo& DiscountInfo::operator=(const DiscountInfo&) = default;
DiscountInfo::~DiscountInfo() = default;

UrlInfo::UrlInfo() = default;
UrlInfo::UrlInfo(const GURL& url,
                 const std::u16string& title,
                 std::optional<GURL> favicon_url,
                 std::optional<GURL> thumbnail_url,
                 std::optional<std::string> previewText)
    : url(url),
      title(title),
      favicon_url(std::move(favicon_url)),
      thumbnail_url(std::move(thumbnail_url)),
      previewText(std::move(previewText)) {}
UrlInfo::UrlInfo(const UrlInfo&) = default;
UrlInfo& UrlInfo::operator=(const UrlInfo& other) = default;
UrlInfo::~UrlInfo() = default;

EntryPointInfo::EntryPointInfo(
    const std::string& title,
    std::map<GURL, uint64_t> similar_candidate_products)
    : title(title),
      similar_candidate_products(std::move(similar_candidate_products)) {}

EntryPointInfo::~EntryPointInfo() = default;
EntryPointInfo::EntryPointInfo(const EntryPointInfo&) = default;
EntryPointInfo& EntryPointInfo::operator=(const EntryPointInfo&) = default;

}  // namespace commerce
