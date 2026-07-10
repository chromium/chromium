// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/structured_metrics_internals_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

StructuredMetricsInternalsHandler::StructuredMetricsInternalsHandler()
    : base_handler_(std::make_unique<
                    metrics::structured::StructuredMetricsInternalsHandlerBase>(
          this,
          g_browser_process->GetMetricsServicesManager()
              ->GetStructuredMetricsService())) {}

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

void StructuredMetricsInternalsHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  ResolveJavascriptCallback(callback_id, response);
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsEvents(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchStructuredMetricsEvents(args[0]);
}

void StructuredMetricsInternalsHandler::HandleFetchStructuredMetricsSummary(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchStructuredMetricsSummary(args[0]);
}
