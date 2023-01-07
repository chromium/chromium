// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://metrics-internals.
class MetricsInternalsHandler : public content::WebUIMessageHandler {
 public:
  MetricsInternalsHandler() = default;

  MetricsInternalsHandler(const MetricsInternalsHandler&) = delete;
  MetricsInternalsHandler& operator=(const MetricsInternalsHandler&) = delete;

  ~MetricsInternalsHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleFetchClientId(const base::Value::List& args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_HANDLER_H_
