// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_

#include <cstddef>

#include "base/metrics/histogram_functions.h"
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
  kMaxValue = kNewDownloadTaskCreated
};

void RecordBDMStartDownloadOutcome(BDMStartDownloadOutcome outcome);

void RecordBDMResultRequestorKnown(bool requestor_known);

void RecordBDWNumJobsCleaned(size_t num_jobs_cleaned);

void RecordBDWStaleDownloadAge(base::TimeDelta download_age);

void RecordBDWExistingJobUsed(bool existing_job_used);

void RecordCRXDownloadComplete(bool had_error);

void RecordCRXDownloaderFallback();

}  // namespace update_client::metrics

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
