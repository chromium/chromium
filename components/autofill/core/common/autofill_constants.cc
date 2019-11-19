// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_constants.h"

#include "build/build_config.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

const char kSettingsOrigin[] = "Chrome settings";

size_t MinRequiredFieldsForHeuristics() {
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics)
             ? 3
             : 1;
}
size_t MinRequiredFieldsForQuery() {
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillEnforceMinRequiredFieldsForQuery)
             ? 3
             : 1;
}
size_t MinRequiredFieldsForUpload() {
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillEnforceMinRequiredFieldsForUpload)
             ? 3
             : 1;
}

bool IsAutofillEntryWithUseDateDeletable(const base::Time& use_date) {
  return use_date < AutofillClock::Now() - kDisusedDataModelDeletionTimeDelta;
}

}  // namespace autofill
