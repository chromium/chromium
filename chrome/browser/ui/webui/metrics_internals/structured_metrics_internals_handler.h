// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/metrics/debug/structured/structured_metrics_internals_handler_base.h"
#include "components/metrics/structured/buildflags/buildflags.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://metrics-internals/structured
class StructuredMetricsInternalsHandler
    : public content::WebUIMessageHandler,
      public metrics::structured::StructuredMetricsInternalsHandlerBase::
          Delegate {
 public:
  StructuredMetricsInternalsHandler();

  StructuredMetricsInternalsHandler(const StructuredMetricsInternalsHandler&) =
      delete;
  StructuredMetricsInternalsHandler& operator=(
      const StructuredMetricsInternalsHandler&) = delete;

  ~StructuredMetricsInternalsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // metrics::structured::StructuredMetricsInternalsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchStructuredMetricsEvents(const base::ListValue& args);
  void HandleFetchStructuredMetricsSummary(const base::ListValue& args);

  std::unique_ptr<metrics::structured::StructuredMetricsInternalsHandlerBase>
      base_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_STRUCTURED_METRICS_INTERNALS_HANDLER_H_
