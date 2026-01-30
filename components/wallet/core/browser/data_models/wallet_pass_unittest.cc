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
  EXPECT_EQ(PassCategoryToString(PassCategory::kPassport), "Passport");
  EXPECT_EQ(PassCategoryToString(PassCategory::kDriverLicense),
            "DriverLicense");
  EXPECT_EQ(PassCategoryToString(PassCategory::kNationalIdentityCard),
            "NationalIdentityCard");
  EXPECT_EQ(PassCategoryToString(PassCategory::kKTN), "KTN");
  EXPECT_EQ(PassCategoryToString(PassCategory::kRedressNumber),
            "RedressNumber");
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

  pass.pass_data = Passport();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kPassport);

  pass.pass_data = DriverLicense();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kDriverLicense);

  pass.pass_data = NationalIdentityCard();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kNationalIdentityCard);

  pass.pass_data = KTN();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kKTN);

  pass.pass_data = RedressNumber();
  EXPECT_EQ(pass.GetPassCategory(), PassCategory::kRedressNumber);
}

}  // namespace wallet
