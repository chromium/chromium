// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// Provides the WebUI message handling for chrome://apc-internals, the
// diagnostics page for Automated Password Change (APC) flows.
class APCInternalsHandler : public content::WebUIMessageHandler {
 public:
  APCInternalsHandler() = default;

  APCInternalsHandler(const APCInternalsHandler&) = delete;
  APCInternalsHandler& operator=(const APCInternalsHandler&) = delete;

  ~APCInternalsHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Creates the initial page. Called when DOMContentLoaded event is observed.
  void OnLoaded(const base::Value::List& args);

  // Data gathering methods.
  // Gathers information on all APC-related feature and feature parameters.
  base::Value::List GetAPCRelatedFlags() const;

  // Gathers information about the script fetcher, e.g. chosen engine,
  // cache state.
  base::Value::Dict GetPasswordScriptFetcherInformation();

  // Gathers AutofillAssistant-related information, e.g. language, locale (can
  // be different from general Chrome settings)
  base::Value::Dict GetAutofillAssistantInformation() const;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
