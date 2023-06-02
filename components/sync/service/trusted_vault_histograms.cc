// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

void RecordTrustedVaultHistogramBooleanWithMigrationSuffix(
    const std::string& histogram_name,
    bool sample,
    const SyncStatus& sync_status) {
  base::UmaHistogramBoolean(histogram_name, sample);

  // Note that the proto field may be unset: in this case the migration time
  // will point to the Unix epoch and will be later ignored.
  const base::Time migration_time =
      ProtoTimeToTime(sync_status.trusted_vault_debug_info.migration_time());
  const base::TimeDelta time_delta_since_migration =
      base::Time::Now() - migration_time;

  if (time_delta_since_migration.is_negative()) {
    return;
  }

  if (time_delta_since_migration < base::Days(180)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast180Days", sample);
  }

  if (time_delta_since_migration < base::Days(90)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast90Days", sample);
  }

  if (time_delta_since_migration < base::Days(28)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast28Days", sample);
  }

  if (time_delta_since_migration < base::Days(7)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast7Days", sample);
  }

  if (time_delta_since_migration < base::Days(3)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLast3Days", sample);
  }

  if (time_delta_since_migration < base::Days(1)) {
    base::UmaHistogramBoolean(histogram_name + ".MigratedLastDay", sample);
  }
}

}  // namespace syncer
