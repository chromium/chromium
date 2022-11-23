// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

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
  void HandleFetchVariationsSummary(const base::Value::List& args);
  void HandleFetchUmaSummary(const base::Value::List& args);
  void HandleFetchUmaLogsData(const base::Value::List& args);
  void OnUmaLogCreatedOrEvent();

  // This UMA log observer keeps track of logs since its creation.
  metrics::MetricsServiceObserver uma_log_observer_;

  // The callback subscription to |uma_log_observer_| that notifies the WebUI
  // of changes. When this subscription is destroyed, it is automatically
  // de-registered from the callback list.
  base::CallbackListSubscription uma_log_notified_subscription_;

  base::WeakPtrFactory<MetricsInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
