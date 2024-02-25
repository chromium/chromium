// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_DATA_CLEANER_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_DATA_CLEANER_METRICS_H_

#include <stddef.h>

namespace autofill::autofill_metrics {

void LogNumberOfProfilesConsideredForDedupe(size_t num_considered);

void LogNumberOfProfilesRemovedDuringDedupe(size_t num_removed);

// Log the number of kLocalOrSyncable Autofill addresses that were because they
// have not been used for `kDisusedDataModelDeletionTimeDelta`.
void LogNumberOfAddressesDeletedForDisuse(size_t num_profiles);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_DATA_CLEANER_METRICS_H_
