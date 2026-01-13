// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_

#include <cstdint>
#include <string>

#include "base/time/time.h"
#include "base/types/strong_alias.h"

namespace autofill {

// Id for valuables coming from Google Wallet.
using ValuableId = base::StrongAlias<class ValuableIdTag, std::string>;

// Contains information about a loyalty card's metadata stored in the
// `valuables_metadata` table.
struct ValuableMetadata {
  ValuableMetadata(ValuableId valuable_id,
                   base::Time use_date,
                   int64_t use_count);
  ValuableMetadata(const ValuableMetadata&);
  ValuableMetadata(ValuableMetadata&&);
  ValuableMetadata& operator=(const ValuableMetadata&) = default;
  ValuableMetadata& operator=(ValuableMetadata&&) = default;

  ~ValuableMetadata();

  ValuableId valuable_id;
  base::Time use_date;
  int64_t use_count;

  friend bool operator==(const ValuableMetadata&,
                         const ValuableMetadata&) = default;
  friend auto operator<=>(const ValuableMetadata&,
                          const ValuableMetadata&) = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_
