// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_

#include <cstddef>

#include "base/time/time.h"

namespace update_client::metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Changes should be reflected in
// "UpdateClientBackgroundDownloaderMacStartDownloadOutcome" in enums.xml.
enum class BDMStartDownloadOutcome {
  kImmediateError = 0,
  kDownloadRecoveredFromCache = 1,
  kSessionHasOngoingDownload = 2,
  kNewDownloadTaskCreated = 3,
  kTooManyTasks = 4,
  kMaxValue = kTooManyTasks
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Changes should be reflected in
// "UpdateClientUpdateCheckResult" in enums.xml.
enum class UpdateCheckResult {
  kError = 0,
  kCanceled = 1,
  kHasUpdate = 2,
  kNoUpdate = 3,
  kMaxValue = kNoUpdate
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Changes should be reflected in
// "UpdateClientCanUpdateResult" in enums.xml.
enum class CanUpdateResult {
  kUpdatesDisabled = 0,
  kCanceled = 1,
  kCheckForUpdateOnly = 2,
  kCanUpdate = 3,
  kMaxValue = kCanUpdate
};

void RecordBDMStartDownloadOutcome(BDMStartDownloadOutcome outcome);

void RecordBDMResultRequestorKnown(bool requestor_known);

void RecordBDWNumJobsCleaned(size_t num_jobs_cleaned);

void RecordBDWStaleDownloadAge(base::TimeDelta download_age);

void RecordBDWExistingJobUsed(bool existing_job_used);

void RecordCRXDownloadComplete(bool had_error);

void RecordCRXDownloaderFallback();

void RecordUpdateCheckResult(UpdateCheckResult result);

void RecordCanUpdateResult(CanUpdateResult result);

void RecordComponentUpdated();

}  // namespace update_client::metrics

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
