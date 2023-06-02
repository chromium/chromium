// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_HISTOGRAMS_H_
#define COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_HISTOGRAMS_H_

#include <string>

namespace syncer {

struct SyncStatus;

void RecordTrustedVaultHistogramBooleanWithMigrationSuffix(
    const std::string& histogram_name,
    bool sample,
    const SyncStatus& sync_status);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_TRUSTED_VAULT_HISTOGRAMS_H_
