// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/wallet_pass.h"

#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/wallet/core/browser/data_models/data_model_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

TEST(WalletPassTest, PassCategoryToString) {
  EXPECT_EQ(PassCategoryToString(PassCategory::kLoyaltyCard), "LoyaltyCard");
  EXPECT_EQ(PassCategoryToString(PassCategory::kEventPass), "EventPass");
  EXPECT_EQ(PassCategoryToString(PassCategory::kTransitTicket),
            "TransitTicket");
  EXPECT_EQ(PassCategoryToString(PassCategory::kBoardingPass), "BoardingPass");
  EXPECT_EQ(PassCategoryToString(PassCategory::kUnspecified), "Unspecified");
}

TEST(WalletPassTest, GetPassCategory) {
  WalletPass pass;

  pass.pass_data = LoyaltyCard();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kLoyaltyCard);

  pass.pass_data = EventPass();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kEventPass);

  pass.pass_data = BoardingPass();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kBoardingPass);

  pass.pass_data = TransitTicket();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kTransitTicket);
}

}  // namespace wallet
