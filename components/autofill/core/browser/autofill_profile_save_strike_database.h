// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/strike_database_integrator_base.h"

class GURL;

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for autofill profile imports.
class AutofillProfileSaveStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  explicit AutofillProfileSaveStrikeDatabase(StrikeDatabase* strike_database);
  ~AutofillProfileSaveStrikeDatabase() override;

  void RemoveStrikesByOriginAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end);

  base::Optional<size_t> GetMaximumEntries() const override;
  base::Optional<size_t> GetMaximumEntriesAfterCleanup() const override;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  base::Optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_
