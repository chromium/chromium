// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_

#include <cstddef>
#include <string>

#include "base/time/time.h"

namespace update_client::metrics {

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

void RecordCRXDownloadComplete(bool had_error);

void RecordUpdateCheckResult(UpdateCheckResult result);

void RecordComponentUpdated();

void RecordCRXDownloadTime(base::TimeDelta time, const std::string& app_id);

void RecordCRXUnzipTime(base::TimeDelta time, const std::string& app_id);

}  // namespace update_client::metrics

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
