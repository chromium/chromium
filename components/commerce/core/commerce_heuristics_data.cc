// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"

#include "base/json/json_reader.h"
#include "base/no_destructor.h"

namespace commerce_heuristics {

namespace {
constexpr char kMerchantNameType[] = "merchant_name";
}  // namespace

// static
CommerceHeuristicsData& CommerceHeuristicsData::GetInstance() {
  static base::NoDestructor<CommerceHeuristicsData> instance;
  return *instance;
}

CommerceHeuristicsData::CommerceHeuristicsData() = default;
CommerceHeuristicsData::~CommerceHeuristicsData() = default;

bool CommerceHeuristicsData::PopulateDataFromComponent(
    const std::string& hint_json_data,
    const std::string& global_json_data,
    const std::string& product_id_json_data,
    const std::string& cart_extraction_script) {
  auto hint_json_value = base::JSONReader::Read(hint_json_data);
  if (!hint_json_value || !hint_json_value.has_value() ||
      !hint_json_value->is_dict()) {
    return false;
  }
  hint_heuristics_ = std::move(*hint_json_value->GetIfDict());
  return true;
}

absl::optional<std::string> CommerceHeuristicsData::GetMerchantName(
    const std::string& domain) {
  return GetCommerceHintHeuristics(kMerchantNameType, domain);
}

absl::optional<std::string> CommerceHeuristicsData::GetCommerceHintHeuristics(
    const std::string& type,
    const std::string& domain) {
  if (!hint_heuristics_.contains(domain)) {
    return absl::nullopt;
  }
  const base::Value::Dict* domain_heuristics =
      hint_heuristics_.FindDict(domain);
  if (!domain_heuristics || domain_heuristics->empty() ||
      !domain_heuristics->contains(type)) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(*domain_heuristics->FindString(type));
}

}  // namespace commerce_heuristics
