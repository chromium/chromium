// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client_metrics.h"

#include <cstddef>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace update_client::metrics {

void RecordCRXDownloadComplete(bool had_error) {
  base::UmaHistogramBoolean(
      "UpdateClient.CrxDownloader.DownloadCompleteSuccess", !had_error);
}

void RecordUpdateCheckResult(UpdateCheckResult result) {
  base::UmaHistogramEnumeration("UpdateClient.Component.UpdateCheckResult",
                                result);
}

void RecordComponentUpdated() {
  base::UmaHistogramBoolean("UpdateClient.Component.Updated", true);
}

}  // namespace update_client::metrics
