// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/structured_metrics_internals_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/debug/structured/structured_metrics_utils.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

StructuredMetricsInternalsHandler::StructuredMetricsInternalsHandler() {
  metrics::structured::StructuredMetricsService* service =
      g_browser_process->GetMetricsServicesManager()
          ->GetStructuredMetricsService();
  if (service) {
    structured_metrics_debug_provider_ =
        std::make_unique<metrics::structured::StructuredMetricsDebugProvider>(
            service);
  }
}

StructuredMetricsInternalsHandler::~StructuredMetricsInternalsHandler() =
    default;

void StructuredMetricsInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchStructuredMetricsEvents",
      base::BindRepeating(&StructuredMetricsInternalsHandler::
                              HandleFetchStructuredMetricsEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchStructuredMetricsSummary",
      base::BindRepeating(&StructuredMetricsInternalsHandler::
                              HandleFetchStructuredMetricsSummary,
                          base::Unretained(this)));
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsEvents(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id, structured_metrics_debug_provider_
                       ? structured_metrics_debug_provider_->events().Clone()
                       : base::Value::List());
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsSummary(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            metrics::structured::GetStructuredMetricsSummary(
                                g_browser_process->GetMetricsServicesManager()
                                    ->GetStructuredMetricsService()));
}
