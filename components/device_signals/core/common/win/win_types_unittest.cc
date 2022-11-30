// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/win/win_types.h"

#include <array>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

TEST(WinTypes, AvProduct_ToValue) {
  std::array<AvProductState, 4> states{
      {AvProductState::kOn, AvProductState::kOff, AvProductState::kSnoozed,
       AvProductState::kExpired}};

  for (const auto state : states) {
    AvProduct av_product{"some display name", state, "some product id"};

    base::Value::Dict expected_value;
    expected_value.Set("displayName", av_product.display_name);
    expected_value.Set("state", static_cast<int>(av_product.state));
    expected_value.Set("productId", av_product.product_id);

    EXPECT_EQ(av_product.ToValue(), base::Value(std::move(expected_value)));
  }
}

TEST(WinTypes, InstalledHotfix_ToValue) {
  InstalledHotfix hotfix{"some hotfix id"};

  base::Value::Dict expected_value;
  expected_value.Set("hotfixId", hotfix.hotfix_id);

  EXPECT_EQ(hotfix.ToValue(), base::Value(std::move(expected_value)));
}

}  // namespace device_signals
