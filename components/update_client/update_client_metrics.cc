// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client_metrics.h"

#include <cstddef>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace update_client::metrics {

void RecordCRXDownloadTime(base::TimeDelta time, const std::string& app_id) {
  base::UmaHistogramLongTimes(
      base::StrCat({"UpdateClient.DownloadTime2.", app_id}), time);
  base::UmaHistogramLongTimes("UpdateClient.DownloadTime2", time);
}

void RecordCRXUnzipTime(base::TimeDelta time, const std::string& app_id) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"UpdateClient.UnzipTime.", app_id}), time);
  base::UmaHistogramMediumTimes("UpdateClient.UnzipTime", time);
}

void RecordCupValidationResult(bool valid) {
  base::UmaHistogramBoolean("UpdateClient.CupValidationResult", valid);
}

void RecordCupValidationTime(base::TimeDelta time) {
  base::UmaHistogramTimes("UpdateClient.CupValidationTime", time);
}

void RecordCupFallbackToEtag(bool fallback_occurred) {
  base::UmaHistogramBoolean("UpdateClient.CupFallbackToEtag",
                            fallback_occurred);
}

}  // namespace update_client::metrics
