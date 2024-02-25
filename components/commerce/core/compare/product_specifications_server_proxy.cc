// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_specifications_server_proxy.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace commerce {

namespace {
const char kProductSpecificationsKey[] = "productSpecifications";
const char kProductSpecificationSectionsKey[] = "productSpecificationSections";
const char kProductSpecificationValuesKey[] = "productSpecificationValues";
const char kKeyKey[] = "key";
const char kTitleKey[] = "title";
const char kIdentifiersKey[] = "identifiers";
const char kGPCKey[] = "gpcId";
const char kMIDKey[] = "mid";
const char kImageURLKey[] = "imageUrl";
const char kDescriptionsKey[] = "descriptions";

}  // namespace

ProductSpecifications::ProductSpecifications() = default;
ProductSpecifications::ProductSpecifications(const ProductSpecifications&) =
    default;
ProductSpecifications::~ProductSpecifications() = default;

ProductSpecifications::Product::Product() = default;
ProductSpecifications::Product::Product(const ProductSpecifications::Product&) =
    default;
ProductSpecifications::Product::~Product() = default;

ProductSpecificationsServerProxy::ProductSpecificationsServerProxy() = default;

ProductSpecificationsServerProxy::~ProductSpecificationsServerProxy() = default;

void ProductSpecificationsServerProxy::GetProductSpecificationsForClusterIds(
    std::vector<uint64_t> cluster_ids,
    base::OnceCallback<void(std::vector<uint64_t>, ProductSpecifications)>) {
  // TODO(b:324442029): Implement full flow
}

std::optional<ProductSpecifications>
ProductSpecificationsServerProxy::ProductSpecificationsFromJsonResponse(
    const base::Value& compareJson) {
  if (!compareJson.is_dict()) {
    return std::nullopt;
  }

  std::optional<ProductSpecifications> product_specs;
  product_specs.emplace();

  const base::Value::Dict* product_specs_dict =
      compareJson.GetDict().FindDict(kProductSpecificationsKey);
  if (!product_specs_dict) {
    return std::nullopt;
  }

  const base::Value::List* spec_sections =
      product_specs_dict->FindList(kProductSpecificationSectionsKey);
  if (!spec_sections) {
    return std::nullopt;
  }

  // Extract the dimensions that the products will be compared by.
  for (const base::Value& section : *spec_sections) {
    if (!section.is_dict()) {
      continue;
    }

    uint64_t section_id;
    const std::string* key_value = section.GetDict().FindString(kKeyKey);
    if (key_value && base::StringToUint64(*key_value, &section_id)) {
      const std::string* title_value = section.GetDict().FindString(kTitleKey);
      product_specs->product_dimension_map[section_id] = *title_value;
    }
  }

  const base::Value::List* specifications =
      product_specs_dict->FindList(kProductSpecificationsKey);
  if (!specifications) {
    return std::nullopt;
  }

  // Extract the individual products that are being compared.
  for (const base::Value& spec : *specifications) {
    ProductSpecifications::Product product;

    if (!spec.is_dict()) {
      continue;
    }

    const base::Value::Dict* id_map = spec.GetDict().FindDict(kIdentifiersKey);
    if (!id_map) {
      continue;
    }

    const std::string* mid = id_map->FindString(kMIDKey);
    if (mid) {
      product.mid = *mid;
    }

    uint64_t cluster_id;
    const std::string* cluster_id_string = id_map->FindString(kGPCKey);
    if (cluster_id_string &&
        base::StringToUint64(*cluster_id_string, &cluster_id)) {
      product.product_cluster_id = cluster_id;
    }

    const std::string* title = spec.GetDict().FindString(kTitleKey);
    if (title) {
      product.title = *title;
    }

    const std::string* image_url = spec.GetDict().FindString(kImageURLKey);
    if (image_url) {
      product.image_url = GURL(*image_url);
    }

    const base::Value::List* product_spec_values =
        spec.GetDict().FindList(kProductSpecificationValuesKey);
    if (!product_spec_values) {
      continue;
    }

    // Extract the dimensions that the products are being compared by.
    for (const base::Value& spec_value : *product_spec_values) {
      if (!spec_value.is_dict()) {
        continue;
      }

      const std::string* value_id_string =
          spec_value.GetDict().FindString(kKeyKey);
      uint64_t value_id;
      if (!base::StringToUint64(*value_id_string, &value_id)) {
        continue;
      }

      const base::Value::List* descriptions_list =
          spec_value.GetDict().FindList(kDescriptionsKey);
      if (!descriptions_list) {
        continue;
      }

      std::vector<std::string> descriptions;
      for (const base::Value& description : *descriptions_list) {
        descriptions.push_back(description.GetString());
      }

      product.product_dimension_values[value_id] = descriptions;
    }

    product_specs->products.push_back(product);
  }

  return product_specs;
}

}  // namespace commerce
