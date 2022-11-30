// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_logs_event_manager.h"

namespace metrics {

MetricsLogsEventManager::MetricsLogsEventManager() = default;

MetricsLogsEventManager::~MetricsLogsEventManager() = default;

void MetricsLogsEventManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MetricsLogsEventManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MetricsLogsEventManager::NotifyLogCreated(
    base::StringPiece log_hash,
    base::StringPiece log_data,
    base::StringPiece log_timestamp) {
  for (Observer& observer : observers_)
    observer.OnLogCreated(log_hash, log_data, log_timestamp);
}

void MetricsLogsEventManager::NotifyLogEvent(LogEvent event,
                                             base::StringPiece log_hash,
                                             base::StringPiece message) {
  for (Observer& observer : observers_)
    observer.OnLogEvent(event, log_hash, message);
}

}  // namespace metrics
