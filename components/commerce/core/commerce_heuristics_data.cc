// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/time/time_delta_from_string.h"

namespace commerce_heuristics {

namespace {
// CommerceHintHeuristics types.
constexpr char kMerchantNameType[] = "merchant_name";
constexpr char kMerchantCartURLType[] = "cart_url";
constexpr char kMerchantCartURLRegexType[] = "cart_url_regex";
constexpr char kMerchantCheckoutURLRegexType[] = "checkout_url_regex";
constexpr char kMerchantPurchaseURLRegexType[] = "purchase_url_regex";
constexpr char kSkipAddToCartRegexType[] = "skip_add_to_cart_regex";

// CommerceGlobalHeuristics types.
constexpr char kSkipProductPatternType[] = "sensitive_product_regex";
constexpr char kRuleDiscountPartnerMerchantPatternType[] =
    "rule_discount_partner_merchant_regex";
constexpr char kCouponDiscountPartnerMerchantPatternType[] =
    "coupon_discount_partner_merchant_regex";
constexpr char kNoDiscountMerchantPatternType[] = "no_discount_merchant_regex";
constexpr char kCartPagetURLPatternType[] = "cart_page_url_regex";
constexpr char kCheckoutPageURLPatternType[] = "checkout_page_url_regex";
constexpr char kPurchaseButtonTextPatternType[] = "purchase_button_text_regex";
constexpr char kAddToCartRequestPatternType[] = "add_to_cart_request_regex";
constexpr char kDiscountFetchDelayType[] = "discount_fetch_delay";

}  // namespace

// static
CommerceHeuristicsData& CommerceHeuristicsData::GetInstance() {
  static base::NoDestructor<CommerceHeuristicsData> instance;
  return *instance;
}

CommerceHeuristicsData::CommerceHeuristicsData() = default;
CommerceHeuristicsData::~CommerceHeuristicsData() = default;

void CommerceHeuristicsData::UpdateVersion(base::Version version) {
  version_ = std::move(version);
}

const std::string CommerceHeuristicsData::GetVersion() {
  if (!version_.IsValid())
    return std::string();
  return version_.GetString();
}

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
  global_heuristics_string_ = global_json_data;
  global_heuristics_ = std::move(*global_json_value->GetIfDict());
  // Global regex patterns.
  product_skip_pattern_ = ConstructGlobalRegex(kSkipProductPatternType);
  rule_discount_partner_merchant_pattern_ =
      ConstructGlobalRegex(kRuleDiscountPartnerMerchantPatternType);
  coupon_discount_partner_merchant_pattern_ =
      ConstructGlobalRegex(kCouponDiscountPartnerMerchantPatternType);
  no_discount_merchant_pattern_ =
      ConstructGlobalRegex(kNoDiscountMerchantPatternType);
  cart_url_pattern_ = ConstructGlobalRegex(kCartPagetURLPatternType);
  checkout_url_pattern_ = ConstructGlobalRegex(kCheckoutPageURLPatternType);
  purchase_button_pattern_ =
      ConstructGlobalRegex(kPurchaseButtonTextPatternType);
  add_to_cart_request_pattern_ =
      ConstructGlobalRegex(kAddToCartRequestPatternType);
  product_id_json_ = product_id_json_data;
  cart_extraction_script_ = cart_extraction_script;
  domain_cart_url_pattern_mapping_.clear();
  domain_checkout_url_pattern_mapping_.clear();
  domain_purchase_url_pattern_mapping_.clear();
  domain_skip_add_to_cart_pattern_mapping_.clear();
  return true;
}

std::optional<std::string> CommerceHeuristicsData::GetMerchantName(
    const std::string& domain) {
  return GetCommerceHintHeuristics(kMerchantNameType, domain);
}

std::optional<std::string> CommerceHeuristicsData::GetMerchantCartURL(
    const std::string& domain) {
  return GetCommerceHintHeuristics(kMerchantCartURLType, domain);
}

std::optional<std::string>
CommerceHeuristicsData::GetHintHeuristicsJSONForDomain(
    const std::string& domain) {
  if (!hint_heuristics_.contains(domain)) {
    return std::nullopt;
  }
  base::Value::Dict domain_heuristics =
      hint_heuristics_.FindDict(domain)->Clone();
  if (domain_heuristics.empty()) {
    return std::nullopt;
  }
  base::Value::Dict res_dic;
  res_dic.Set(domain, std::move(domain_heuristics));
  std::string res_string;
  base::JSONWriter::Write(res_dic, &res_string);
  return std::optional<std::string>(res_string);
}

std::optional<std::string> CommerceHeuristicsData::GetGlobalHeuristicsJSON() {
  return global_heuristics_string_;
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

const re2::RE2* CommerceHeuristicsData::GetNoDiscountMerchantPattern() {
  return no_discount_merchant_pattern_.get();
}

const re2::RE2* CommerceHeuristicsData::GetCartPageURLPattern() {
  return cart_url_pattern_.get();
}

const re2::RE2* CommerceHeuristicsData::GetCheckoutPageURLPattern() {
  return checkout_url_pattern_.get();
}

const re2::RE2* CommerceHeuristicsData::GetPurchaseButtonTextPattern() {
  return purchase_button_pattern_.get();
}

const re2::RE2* CommerceHeuristicsData::GetAddToCartRequestPattern() {
  return add_to_cart_request_pattern_.get();
}

const re2::RE2* CommerceHeuristicsData::GetCartPageURLPatternForDomain(
    const std::string& domain) {
  return GetCommerceHintHeuristicsRegex(domain_cart_url_pattern_mapping_,
                                        kMerchantCartURLRegexType, domain);
}

const re2::RE2* CommerceHeuristicsData::GetCheckoutPageURLPatternForDomain(
    const std::string& domain) {
  return GetCommerceHintHeuristicsRegex(domain_checkout_url_pattern_mapping_,
                                        kMerchantCheckoutURLRegexType, domain);
}

const re2::RE2* CommerceHeuristicsData::GetPurchasePageURLPatternForDomain(
    const std::string& domain) {
  return GetCommerceHintHeuristicsRegex(domain_purchase_url_pattern_mapping_,
                                        kMerchantPurchaseURLRegexType, domain);
}

const re2::RE2* CommerceHeuristicsData::GetSkipAddToCartPatternForDomain(
    const std::string& domain) {
  return GetCommerceHintHeuristicsRegex(
      domain_skip_add_to_cart_pattern_mapping_, kSkipAddToCartRegexType,
      domain);
}

std::string CommerceHeuristicsData::GetProductIDExtractionJSON() {
  return product_id_json_;
}

std::string CommerceHeuristicsData::GetCartProductExtractionScript() {
  return cart_extraction_script_;
}

std::optional<base::TimeDelta> CommerceHeuristicsData::GetDiscountFetchDelay() {
  auto delay_value_optional =
      GetCommerceGlobalHeuristics(kDiscountFetchDelayType);
  if (!delay_value_optional.has_value()) {
    return std::nullopt;
  }
  return base::TimeDeltaFromString(*delay_value_optional);
}

std::optional<std::string> CommerceHeuristicsData::GetCommerceHintHeuristics(
    const std::string& type,
    const std::string& domain) {
  if (!hint_heuristics_.contains(domain)) {
    return std::nullopt;
  }
  const base::Value::Dict* domain_heuristics =
      hint_heuristics_.FindDict(domain);
  if (!domain_heuristics || domain_heuristics->empty() ||
      !domain_heuristics->contains(type)) {
    return std::nullopt;
  }
  return std::optional<std::string>(*domain_heuristics->FindString(type));
}

std::optional<std::string> CommerceHeuristicsData::GetCommerceGlobalHeuristics(
    const std::string& type) {
  if (!global_heuristics_.contains(type)) {
    return std::nullopt;
  }
  return std::optional<std::string>(*global_heuristics_.FindString(type));
}

const re2::RE2* CommerceHeuristicsData::GetCommerceHintHeuristicsRegex(
    std::map<std::string, std::unique_ptr<re2::RE2>>& map,
    const std::string type,
    const std::string domain) {
  if (map.find(domain) != map.end())
    return map.at(domain).get();
  std::optional<std::string> pattern = GetCommerceHintHeuristics(type, domain);
  if (!pattern.has_value())
    return nullptr;
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  map.emplace(domain, std::make_unique<re2::RE2>(*pattern, options));
  return map.at(domain).get();
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
