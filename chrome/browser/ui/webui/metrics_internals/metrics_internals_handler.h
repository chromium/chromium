// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/metrics/debug/structured/structured_metrics_watcher.h"
#endif

// UI Handler for chrome://metrics-internals.
class MetricsInternalsHandler : public content::WebUIMessageHandler {
 public:
  MetricsInternalsHandler();

  MetricsInternalsHandler(const MetricsInternalsHandler&) = delete;
  MetricsInternalsHandler& operator=(const MetricsInternalsHandler&) = delete;

  ~MetricsInternalsHandler() override;

  // content::WebUIMessageHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 private:
  // Returns true if the metrics service has its own logs event observer, which
  // indicates that we should use that observer instead of our own. This happens
  // if this is a debug build, or if the |kExportUmaLogsToFile| command line
  // flag is present.
  bool ShouldUseMetricsServiceObserver();

  // Returns the UMA observer to use for tracking UMA logs. I.e., if the metrics
  // service has its own observer, return that one. Otherwise, return the one
  // owned by the WebUI page (|uma_log_observer_|).
  metrics::MetricsServiceObserver* GetUmaObserver();

  void HandleFetchVariationsSummary(const base::Value::List& args);
  void HandleFetchUmaSummary(const base::Value::List& args);
  void HandleFetchUmaLogsData(const base::Value::List& args);
  void HandleIsUsingMetricsServiceObserver(const base::Value::List& args);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void HandleFetchStructuredMetricsEvents(const base::Value::List& args);
  void HandleFetchStructuredMetricsSummary(const base::Value::List& args);
#endif

  void OnUmaLogCreatedOrEvent();

  // This UMA log observer keeps track of logs since its creation. It is unused
  // if the UMA metrics service has its own observer that has observed all
  // events since browser startup.
  std::unique_ptr<metrics::MetricsServiceObserver> uma_log_observer_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<metrics::structured::StructuredMetricsWatcher>
      structured_metrics_watcher_;
#endif

  // The callback subscription to |uma_log_observer_| that notifies the WebUI
  // of changes. When this subscription is destroyed, it is automatically
  // de-registered from the callback list.
  base::CallbackListSubscription uma_log_notified_subscription_;

  base::WeakPtrFactory<MetricsInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
