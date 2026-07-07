// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_INTERNALS_HANDLER_BASE_H_
#define COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_INTERNALS_HANDLER_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/metrics/debug/structured/structured_metrics_debug_provider.h"
#include "components/metrics/structured/structured_metrics_service.h"

namespace metrics::structured {

// Platform-agnostic logic for chrome://metrics-internals/structured.
class StructuredMetricsInternalsHandlerBase {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void ResolvePageCallback(const base::ValueView callback_id,
                                     const base::ValueView response) = 0;
  };

  StructuredMetricsInternalsHandlerBase(Delegate* delegate,
                                        StructuredMetricsService* service);

  StructuredMetricsInternalsHandlerBase(
      const StructuredMetricsInternalsHandlerBase&) = delete;
  StructuredMetricsInternalsHandlerBase& operator=(
      const StructuredMetricsInternalsHandlerBase&) = delete;

  ~StructuredMetricsInternalsHandlerBase();

  void HandleFetchStructuredMetricsEvents(const base::Value& callback_id);
  void HandleFetchStructuredMetricsSummary(const base::Value& callback_id);

 private:
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<StructuredMetricsService> service_;

  // Interface for providing events to the debug page.
  std::unique_ptr<StructuredMetricsDebugProvider>
      structured_metrics_debug_provider_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_INTERNALS_HANDLER_BASE_H_
