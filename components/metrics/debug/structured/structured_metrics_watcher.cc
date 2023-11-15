// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/structured/structured_metrics_watcher.h"

#include "base/logging.h"
#include "components/metrics/structured/structured_metrics_service.h"

namespace metrics::structured {

StructuredMetricsWatcher::StructuredMetricsWatcher(
    StructuredMetricsService* service)
    : service_(service) {
  Recorder::GetInstance()->AddObserver(this);
}

StructuredMetricsWatcher::~StructuredMetricsWatcher() {
  Recorder::GetInstance()->RemoveObserver(this);
}

void StructuredMetricsWatcher::OnEventRecord(const Event& event) {
  if (!service_->recording_enabled()) {
    return;
  }

  events_.push_back(event.Clone());
}

void StructuredMetricsWatcher::OnProfileAdded(
    const base::FilePath& profile_path) {
  /* Do nothing */
}

void StructuredMetricsWatcher::OnReportingStateChanged(bool enabled) {
  /* Do nothing */
}

}  // namespace metrics::structured
