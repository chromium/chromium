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

MetricsLogManager::MetricsLogManager() {}

MetricsLogManager::~MetricsLogManager() {}

void MetricsLogManager::BeginLoggingWithLog(std::unique_ptr<MetricsLog> log) {
  DCHECK(!current_log_);
  current_log_ = std::move(log);
}

void MetricsLogManager::FinishCurrentLog(MetricsLogStore* log_store) {
  DCHECK(current_log_);
  current_log_->RecordLogWrittenByAppVersionIfNeeded();
  current_log_->CloseLog();
  std::string log_data;
  current_log_->GetEncodedLog(&log_data);
  if (!log_data.empty()) {
    log_store->StoreLog(log_data, current_log_->log_type(),
                        current_log_->log_metadata());
  }
  current_log_.reset();
}

void MetricsLogManager::DiscardCurrentLog() {
  current_log_->CloseLog();
  current_log_.reset();
}

void MetricsLogManager::PauseCurrentLog() {
  DCHECK(!paused_log_);
  paused_log_ = std::move(current_log_);
}

void MetricsLogManager::ResumePausedLog() {
  DCHECK(!current_log_);
  current_log_ = std::move(paused_log_);
}

}  // namespace metrics
