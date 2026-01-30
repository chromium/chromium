// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/data_model_utils.h"

#include "base/notreached.h"

namespace wallet {

// LINT.IfChange(PassCategoryToString)
std::string PassCategoryToString(PassCategory category) {
  switch (category) {
    case PassCategory::kLoyaltyCard:
      return "LoyaltyCard";
    case PassCategory::kEventPass:
      return "EventPass";
    case PassCategory::kTransitTicket:
      return "TransitTicket";
    case PassCategory::kBoardingPass:
      return "BoardingPass";
    case PassCategory::kPassport:
      return "Passport";
    case PassCategory::kDriverLicense:
      return "DriverLicense";
    case PassCategory::kNationalIdentityCard:
      return "NationalIdentityCard";
    case PassCategory::kKTN:
      return "KTN";
    case PassCategory::kRedressNumber:
      return "RedressNumber";
    case PassCategory::kUnspecified:
      return "Unspecified";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/histograms.xml:Wallet.WalletablePass.PassCategory)

}  // namespace wallet
