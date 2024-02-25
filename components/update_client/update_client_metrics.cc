// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client_metrics.h"

#include <cstddef>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace update_client::metrics {

void RecordBDMStartDownloadOutcome(BDMStartDownloadOutcome outcome) {
  base::UmaHistogramEnumeration(
      "UpdateClient.BackgroundDownloaderMac.StartDownloadOutcome", outcome);
}

void RecordBDMResultRequestorKnown(bool requestor_known) {
  base::UmaHistogramBoolean(
      "UpdateClient.BackgroundDownloaderMac.DownloadResultRequestorKnown",
      requestor_known);
}

void RecordBDWNumJobsCleaned(size_t num_jobs_cleaned) {
  base::UmaHistogramCounts100(
      "UpdateClient.BackgroundDownloaderWin.StaleJobsCleaned",
      num_jobs_cleaned);
}

void RecordBDWStaleDownloadAge(base::TimeDelta download_age) {
  base::UmaHistogramCustomCounts(
      "UpdateClient.BackgroundDownloaderWin.StaleDownloadAge",
      download_age.InHours(), 0, base::Days(30).InHours(), 50);
}

void RecordBDWExistingJobUsed(bool existing_job_used) {
  base::UmaHistogramBoolean(
      "UpdateClient.BackgroundDownloaderWin.ExistingJobUsed",
      existing_job_used);
}

void RecordCRXDownloadComplete(bool had_error) {
  base::UmaHistogramBoolean(
      "UpdateClient.CrxDownloader.DownloadCompleteSuccess", !had_error);
}

void RecordCRXDownloaderFallback() {
  base::UmaHistogramBoolean("UpdateClient.CrxDownloader.Fallback", true);
}

void RecordUpdateCheckResult(UpdateCheckResult result) {
  base::UmaHistogramEnumeration("UpdateClient.Component.UpdateCheckResult",
                                result);
}

void RecordCanUpdateResult(CanUpdateResult result) {
  base::UmaHistogramEnumeration("UpdateClient.Component.CanUpdateResult",
                                result);
}

void RecordComponentUpdated() {
  base::UmaHistogramBoolean("UpdateClient.Component.Updated", true);
}

}  // namespace update_client::metrics
