// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_HOST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_HOST_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/strike_databases/history_clearable_strike_database.h"

namespace autofill {

struct AutofillAiSaveStrikeDatabaseByHostTraits {
  static constexpr std::string_view kName = "AutofillEntitySaveByHost";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;

  // Retrieves the host from the `id` by essentially reverting
  // `AutofillAiSaveStrikeDatabaseByHost::GetId`.
  static std::string HostFromId(const std::string& id);
};

class AutofillAiSaveStrikeDatabaseByHost
    : public autofill::HistoryClearableStrikeDatabase<
          AutofillAiSaveStrikeDatabaseByHostTraits> {
 public:
  using autofill::HistoryClearableStrikeDatabase<
      AutofillAiSaveStrikeDatabaseByHostTraits>::HistoryClearableStrikeDatabase;

  // Returns an id for use in the strike database.
  static std::string GetId(std::string_view entity_name, std::string_view host);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_HOST_H_
