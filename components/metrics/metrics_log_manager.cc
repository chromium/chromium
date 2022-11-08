// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_manager.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_util.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_pref_names.h"

namespace metrics {

MetricsLogManager::MetricsLogManager() = default;

MetricsLogManager::~MetricsLogManager() = default;

void MetricsLogManager::BeginLoggingWithLog(std::unique_ptr<MetricsLog> log) {
  DCHECK(!current_log_);
  current_log_ = std::move(log);
}

std::unique_ptr<MetricsLog> MetricsLogManager::ReleaseCurrentLog() {
  DCHECK(current_log_);
  return std::move(current_log_);
}

}  // namespace metrics
