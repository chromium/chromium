// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/metrics/debug/structured/structured_metrics_debug_provider.h"
#include "components/metrics/structured/buildflags/buildflags.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://metrics-internals/structured
class StructuredMetricsInternalsHandler : public content::WebUIMessageHandler {
 public:
  StructuredMetricsInternalsHandler();

  StructuredMetricsInternalsHandler(const StructuredMetricsInternalsHandler&) =
      delete;
  StructuredMetricsInternalsHandler& operator=(
      const StructuredMetricsInternalsHandler&) = delete;

  ~StructuredMetricsInternalsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleFetchStructuredMetricsEvents(const base::Value::List& args);
  void HandleFetchStructuredMetricsSummary(const base::Value::List& args);

  // Interface for providing events to the debug page.
  std::unique_ptr<metrics::structured::StructuredMetricsDebugProvider>
      structured_metrics_debug_provider_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
