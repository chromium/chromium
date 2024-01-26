// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_client.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "components/metrics/structured/event.h"

namespace metrics::structured {

StructuredMetricsClient::StructuredMetricsClient() = default;
StructuredMetricsClient::~StructuredMetricsClient() = default;

// static
StructuredMetricsClient* StructuredMetricsClient::Get() {
  static base::NoDestructor<StructuredMetricsClient> client;
  return client.get();
}

// static
void StructuredMetricsClient::Record(Event&& event) {
  StructuredMetricsClient::Get()->RecordEvent(std::move(event));
}

void StructuredMetricsClient::RecordEvent(Event&& event) {
  // Records uptime if event sequence type and it has not been explicitly set.
  if (event.IsEventSequenceType() && !event.has_system_uptime()) {
    event.SetRecordedTimeSinceBoot(base::SysInfo::Uptime());
  }

  if (delegate_ && delegate_->IsReadyToRecord()) {
    delegate_->RecordEvent(std::move(event));
  }
}

void StructuredMetricsClient::SetDelegate(RecordingDelegate* delegate) {
  delegate_ = delegate;
}

void StructuredMetricsClient::UnsetDelegate() {
  delegate_ = nullptr;
}

}  // namespace metrics::structured
