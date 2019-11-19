// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_

#include <string>

#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/payments/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for credit card saves (both
// local and upload).
class CreditCardSaveStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  CreditCardSaveStrikeDatabase(StrikeDatabase* strike_database);
  ~CreditCardSaveStrikeDatabase() override;

  std::string GetProjectPrefix() override;
  int GetMaxStrikesLimit() override;
  long long GetExpiryTimeMicros() override;
  bool UniqueIdsRequired() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
