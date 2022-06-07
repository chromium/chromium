// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_

#include "base/values.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
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

  // Responds to requests for script cache updates. Called by user-triggered
  // DOM event.
  void OnScriptCacheRequested(const base::Value::List& args);

  // Responds to requests for refreshing script cache by prewarming the cache.
  // Called by user-triggered DOM event.
  void OnRefreshScriptCacheRequested(const base::Value::List& args);

  // Fires "on-autofill-assistant-information-received" to update Autofill
  // Assistant Information on the page.
  void UpdateAutofillAssistantInformation();

  // Responds to requests for setting the Autofill Assistant URL. Called by
  // user-triggered DOM event.
  void OnSetAutofillAssistantUrl(const base::Value::List& args);

  // Returns a raw pointer to the |PasswordScriptsFetcher| keyed service.
  password_manager::PasswordScriptsFetcher* GetPasswordScriptsFetcher();

  // Data gathering methods.
  // Gathers information on all APC-related feature and feature parameters.
  base::Value::List GetAPCRelatedFlags() const;

  // Gathers information about the script fetcher, e.g. chosen engine,
  // cache state.
  base::Value::Dict GetPasswordScriptFetcherInformation();

  // Retrieves the current state of the password script fetcher cache.
  base::Value::List GetPasswordScriptFetcherCache();

  // Gathers AutofillAssistant-related information, e.g. language, locale (can
  // be different from general Chrome settings)
  base::Value::Dict GetAutofillAssistantInformation() const;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
