// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_

#include <memory>

#include "components/metrics/debug/metrics_internals_handler_base.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://metrics-internals.
class MetricsInternalsHandler
    : public content::WebUIMessageHandler,
      public metrics::MetricsInternalsHandlerBase::Delegate {
 public:
  MetricsInternalsHandler();

  MetricsInternalsHandler(const MetricsInternalsHandler&) = delete;
  MetricsInternalsHandler& operator=(const MetricsInternalsHandler&) = delete;

  ~MetricsInternalsHandler() override;

  // content::WebUIMessageHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // metrics::MetricsInternalsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;
  void FireWebUIListener(std::string_view event_name) override;
  void FireWebUIListener(std::string_view event_name,
                         const base::ValueView arg1) override;

 private:
  void HandleFetchVariationsSummary(const base::ListValue& args);
  void HandleFetchStoredSeedInfo(
      variations::VariationsSeedStore::SeedType seed_type,
      const base::ListValue& args);
  void HandleFetchUmaSummary(const base::ListValue& args);
  void HandleFetchUmaLogsData(const base::ListValue& args);
  void HandleFetchEncryptionPublicKey(const base::ListValue& args);
  void HandleIsUsingMetricsServiceObserver(const base::ListValue& args);

  std::unique_ptr<metrics::MetricsInternalsHandlerBase> base_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
