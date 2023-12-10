// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_metadata.h"

#include <ostream>

#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"

namespace autofill {

bool AutofillMetadata::IsDeletable() const {
  return IsAutofillEntryWithUseDateDeletable(use_date);
}

std::ostream& operator<<(std::ostream& os, const AutofillMetadata& metadata) {
  return os << metadata.id << " " << metadata.use_count << " "
            << metadata.use_date << " " << metadata.billing_address_id;
}

}  // namespace autofill
