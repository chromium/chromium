// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/structured/structured_metrics_internals_handler_base.h"

#include "base/values.h"
#include "components/metrics/debug/structured/structured_metrics_utils.h"

namespace metrics::structured {

StructuredMetricsInternalsHandlerBase::StructuredMetricsInternalsHandlerBase(
    Delegate* delegate,
    StructuredMetricsService* service)
    : delegate_(delegate), service_(service) {
  if (service_) {
    structured_metrics_debug_provider_ =
        std::make_unique<StructuredMetricsDebugProvider>(service_);
  }
}

StructuredMetricsInternalsHandlerBase::
    ~StructuredMetricsInternalsHandlerBase() = default;

void StructuredMetricsInternalsHandlerBase::HandleFetchStructuredMetricsEvents(
    const base::Value& callback_id) {
  const base::ListValue empty_events;
  delegate_->ResolvePageCallback(
      callback_id, structured_metrics_debug_provider_
                       ? structured_metrics_debug_provider_->events()
                       : empty_events);
}

void StructuredMetricsInternalsHandlerBase::HandleFetchStructuredMetricsSummary(
    const base::Value& callback_id) {
  delegate_->ResolvePageCallback(callback_id,
                                 GetStructuredMetricsSummary(service_));
}

}  // namespace metrics::structured
