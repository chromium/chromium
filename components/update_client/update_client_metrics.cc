// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client_metrics.h"

#include "base/metrics/histogram_functions.h"

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

}  // namespace update_client::metrics
