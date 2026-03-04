// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_

#include <cstddef>
#include <string>

#include "base/time/time.h"

namespace update_client::metrics {

void RecordCRXDownloadTime(base::TimeDelta time, const std::string& app_id);

void RecordCRXUnzipTime(base::TimeDelta time, const std::string& app_id);

}  // namespace update_client::metrics

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
