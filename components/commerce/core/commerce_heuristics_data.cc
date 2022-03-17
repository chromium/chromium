// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"

#include "base/json/json_reader.h"
#include "base/no_destructor.h"

namespace commerce_heuristics {

namespace {
// CommerceHintHeuristics types.
constexpr char kMerchantNameType[] = "merchant_name";
constexpr char kMerchantCartURLType[] = "cart_url";

// CommerceGlobalHeuristics types.
constexpr char kSkipProductPatternType[] = "sensitive_product_regex";
constexpr char kRuleDiscountPartnerMerchantPatternType[] =
    "rule_discount_partner_merchant_regex";
constexpr char kCouponDiscountPartnerMerchantPatternType[] =
    "coupon_discount_partner_merchant_regex";

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
  auto global_json_value = base::JSONReader::Read(global_json_data);
  if (!hint_json_value || !hint_json_value.has_value() ||
      !hint_json_value->is_dict()) {
    return false;
  }
  if (!global_json_value || !global_json_value.has_value() ||
      !global_json_value->is_dict()) {
    return false;
  }
  hint_heuristics_ = std::move(*hint_json_value->GetIfDict());
  global_heuristics_ = std::move(*global_json_value->GetIfDict());
  product_skip_pattern_ = ConstructGlobalRegex(kSkipProductPatternType);
  rule_discount_partner_merchant_pattern_ =
      ConstructGlobalRegex(kRuleDiscountPartnerMerchantPatternType);
  coupon_discount_partner_merchant_pattern_ =
      ConstructGlobalRegex(kCouponDiscountPartnerMerchantPatternType);
  return true;
}

absl::optional<std::string> CommerceHeuristicsData::GetMerchantName(
    const std::string& domain) {
  return GetCommerceHintHeuristics(kMerchantNameType, domain);
}

absl::optional<std::string> CommerceHeuristicsData::GetMerchantCartURL(
    const std::string& domain) {
  return GetCommerceHintHeuristics(kMerchantCartURLType, domain);
}

const re2::RE2* CommerceHeuristicsData::GetProductSkipPattern() {
  return product_skip_pattern_.get();
}

const re2::RE2*
CommerceHeuristicsData::GetRuleDiscountPartnerMerchantPattern() {
  return rule_discount_partner_merchant_pattern_.get();
}

const re2::RE2*
CommerceHeuristicsData::GetCouponDiscountPartnerMerchantPattern() {
  return coupon_discount_partner_merchant_pattern_.get();
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

absl::optional<std::string> CommerceHeuristicsData::GetCommerceGlobalHeuristics(
    const std::string& type) {
  if (!global_heuristics_.contains(type)) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(*global_heuristics_.FindString(type));
}

std::unique_ptr<re2::RE2> CommerceHeuristicsData::ConstructGlobalRegex(
    const std::string& type) {
  if (!GetCommerceGlobalHeuristics(type).has_value()) {
    return nullptr;
  }
  std::string pattern = *GetCommerceGlobalHeuristics(type);
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  return std::make_unique<re2::RE2>(std::move(pattern), options);
}

}  // namespace commerce_heuristics
