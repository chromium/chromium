// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_

#include <string>
#include "base/values.h"
#include "third_party/re2/src/re2/re2.h"

namespace commerce_heuristics {

class CommerceHeuristicsData {
 public:
  static CommerceHeuristicsData& GetInstance();

  CommerceHeuristicsData();
  CommerceHeuristicsData(const CommerceHeuristicsData&) = delete;
  CommerceHeuristicsData& operator=(const CommerceHeuristicsData&) = delete;
  ~CommerceHeuristicsData();

  // Populate and cache the heuristics from JSON data.
  bool PopulateDataFromComponent(const std::string& hint_json_data,
                                 const std::string& global_json_data,
                                 const std::string& product_id_json_data,
                                 const std::string& cart_extraction_script);
  // Try to get merchant name for `domain`.
  absl::optional<std::string> GetMerchantName(const std::string& domain);

  // Try to get merchant cart URL for `domain`.
  absl::optional<std::string> GetMerchantCartURL(const std::string& domain);

  // Try to get the product skip pattern.
  const re2::RE2* GetProductSkipPattern();

  // Try to get the pattern regex to decide if a merchant is a partner merchant
  // for rule discount.
  const re2::RE2* GetRuleDiscountPartnerMerchantPattern();

  // Try to get the pattern regex to decide if a merchant is a partner merchant
  // for coupon discount.
  const re2::RE2* GetCouponDiscountPartnerMerchantPattern();

 private:
  friend class CommerceHeuristicsDataTest;

  absl::optional<std::string> GetCommerceHintHeuristics(
      const std::string& type,
      const std::string& domain);

  absl::optional<std::string> GetCommerceGlobalHeuristics(
      const std::string& type);

  std::unique_ptr<re2::RE2> ConstructGlobalRegex(const std::string& type);

  base::Value::Dict hint_heuristics_;
  base::Value::Dict global_heuristics_;
  std::unique_ptr<re2::RE2> product_skip_pattern_;
  std::unique_ptr<re2::RE2> rule_discount_partner_merchant_pattern_;
  std::unique_ptr<re2::RE2> coupon_discount_partner_merchant_pattern_;
};

}  // namespace commerce_heuristics

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
