// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"

#include "base/no_destructor.h"

namespace commerce_heuristics {

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
  // TODO(crbug.com/1300332): Parse the data.
  return false;
}

}  // namespace commerce_heuristics
