// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for IBAN save strike check.
// Owned by IbanSaveManager.
class IbanSaveStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  explicit IbanSaveStrikeDatabase(StrikeDatabase* strike_database);

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  absl::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
