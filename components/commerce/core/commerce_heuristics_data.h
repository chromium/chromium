// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_

#include <string>
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "third_party/re2/src/re2/re2.h"

namespace commerce_heuristics {

class CommerceHeuristicsData {
 public:
  static CommerceHeuristicsData& GetInstance();

  CommerceHeuristicsData();
  CommerceHeuristicsData(const CommerceHeuristicsData&) = delete;
  CommerceHeuristicsData& operator=(const CommerceHeuristicsData&) = delete;
  ~CommerceHeuristicsData();

  // Called by component installer to update the version number of the
  // heuristics.
  void UpdateVersion(base::Version version);

  // Get the current version number of the heuristics.
  const std::string GetVersion();

  // Populate and cache the heuristics from JSON data.
  bool PopulateDataFromComponent(const std::string& hint_json_data,
                                 const std::string& global_json_data,
                                 const std::string& product_id_json_data,
                                 const std::string& cart_extraction_script);
  // Try to get merchant name for `domain`.
  std::optional<std::string> GetMerchantName(const std::string& domain);

  // Try to get merchant cart URL for `domain`.
  std::optional<std::string> GetMerchantCartURL(const std::string& domain);

  // Try to get hint heuristics JSON data for `domain`.
  std::optional<std::string> GetHintHeuristicsJSONForDomain(
      const std::string& domain);

  // Try to get global heuristics JSON data.
  std::optional<std::string> GetGlobalHeuristicsJSON();

  // Try to get the product skip pattern.
  const re2::RE2* GetProductSkipPattern();

  // Try to get the pattern regex to decide if a merchant is a partner merchant
  // for rule discount.
  const re2::RE2* GetRuleDiscountPartnerMerchantPattern();

  // Try to get the pattern regex to decide if a merchant is a partner merchant
  // for coupon discount.
  const re2::RE2* GetCouponDiscountPartnerMerchantPattern();

  // Try to get the pattern regex to decide if a merchant is one the merchants
  // that currently have no discounts. This pattern is determined on the server
  // side.
  const re2::RE2* GetNoDiscountMerchantPattern();

  // Try to get the pattern regex to decide if a URL is cart page URL.
  const re2::RE2* GetCartPageURLPattern();

  // Try to get the pattern regex to decide if a URL is checkout page URL.
  const re2::RE2* GetCheckoutPageURLPattern();

  // Try to get the pattern regex to decide if a button is a purchase button.
  const re2::RE2* GetPurchaseButtonTextPattern();

  // Try to get the pattern regex to decide if a request is a add-to-cart
  // request.
  const re2::RE2* GetAddToCartRequestPattern();

  // Try to get the pattern regex to decide if a URL is cart page URL in
  // `domain`.
  const re2::RE2* GetCartPageURLPatternForDomain(const std::string& domain);

  // Try to get the pattern regex to decide if a URL is checkout page URL in
  // `domain`.
  const re2::RE2* GetCheckoutPageURLPatternForDomain(const std::string& domain);

  // Try to get the pattern regex to decide if a URL is purchase page URL in
  // `domain`.
  const re2::RE2* GetPurchasePageURLPatternForDomain(const std::string& domain);

  // Try to get the pattern regex used to match against XHR request URL to see
  // if the request should be ignored for AddToCart detection in `domain`.
  const re2::RE2* GetSkipAddToCartPatternForDomain(const std::string& domain);

  // Get the JSON data with product ID extraction heuristics.
  std::string GetProductIDExtractionJSON();

  // Get the cart extraction script.
  std::string GetCartProductExtractionScript();

  // Get the time delay between discount fetches.
  std::optional<base::TimeDelta> GetDiscountFetchDelay();

 private:
  friend class CommerceHeuristicsDataTest;

  std::optional<std::string> GetCommerceHintHeuristics(
      const std::string& type,
      const std::string& domain);

  std::optional<std::string> GetCommerceGlobalHeuristics(
      const std::string& type);

  const re2::RE2* GetCommerceHintHeuristicsRegex(
      std::map<std::string, std::unique_ptr<re2::RE2>>& map,
      const std::string type,
      const std::string domain);

  std::unique_ptr<re2::RE2> ConstructGlobalRegex(const std::string& type);

  base::Version version_;
  base::Value::Dict hint_heuristics_;
  base::Value::Dict global_heuristics_;
  std::string global_heuristics_string_;
  std::unique_ptr<re2::RE2> product_skip_pattern_;
  std::unique_ptr<re2::RE2> rule_discount_partner_merchant_pattern_;
  std::unique_ptr<re2::RE2> coupon_discount_partner_merchant_pattern_;
  std::unique_ptr<re2::RE2> no_discount_merchant_pattern_;
  std::unique_ptr<re2::RE2> cart_url_pattern_;
  std::unique_ptr<re2::RE2> checkout_url_pattern_;
  std::unique_ptr<re2::RE2> purchase_button_pattern_;
  std::unique_ptr<re2::RE2> add_to_cart_request_pattern_;
  std::map<std::string, std::unique_ptr<re2::RE2>>
      domain_cart_url_pattern_mapping_;
  std::map<std::string, std::unique_ptr<re2::RE2>>
      domain_checkout_url_pattern_mapping_;
  std::map<std::string, std::unique_ptr<re2::RE2>>
      domain_purchase_url_pattern_mapping_;
  std::map<std::string, std::unique_ptr<re2::RE2>>
      domain_skip_add_to_cart_pattern_mapping_;
  std::string product_id_json_;
  std::string cart_extraction_script_;
};

}  // namespace commerce_heuristics

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
