// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_

#include <string>
#include "base/values.h"

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

 private:
  friend class CommerceHeuristicsDataTest;

  absl::optional<std::string> GetCommerceHintHeuristics(
      const std::string& type,
      const std::string& domain);

  base::Value::Dict hint_heuristics_;
};

}  // namespace commerce_heuristics

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_H_
