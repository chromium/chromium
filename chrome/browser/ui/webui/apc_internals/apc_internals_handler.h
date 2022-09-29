// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class APCInternalsLoginsRequest;

namespace password_manager {
class PasswordScriptsFetcher;
class PasswordStoreInterface;
}  // namespace password_manager

// Provides the WebUI message handling for chrome://apc-internals, the
// diagnostics page for Automated Password Change (APC) flows.
class APCInternalsHandler : public content::WebUIMessageHandler {
 public:
  APCInternalsHandler();

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

  // Fires "on-prefs-information-received" to update preference information on
  // the page.
  void UpdatePrefsInformation();

  // Responds to requests to toggle a user pref.
  void OnToggleUserPref(const base::Value::List& args);

  // Responds to requests to remove a user set valule for a pref.
  void OnRemoveUserPref(const base::Value::List& args);

  // Fires "on-autofill-assistant-information-received" to update Autofill
  // Assistant information on the page.
  void UpdateAutofillAssistantInformation();

  // Responds to requests for setting the Autofill Assistant URL. Called by
  // user-triggered DOM event.
  void OnSetAutofillAssistantUrl(const base::Value::List& args);

  void GetLoginsAndTryLaunchScript(const base::Value::List& args);

  // Returns a raw pointer to the `PasswordScriptsFetcher` keyed service.
  password_manager::PasswordScriptsFetcher* GetPasswordScriptsFetcher();

  // Data gathering methods.
  // Gathers information on all APC-related features and feature parameters.
  base::Value::List GetAPCRelatedFlags() const;

  // Gathers information on all APC-related prefs.
  base::Value::List GetAPCRelatedPrefs();

  // Gathers information about the script fetcher, e.g. chosen engine,
  // cache state.
  base::Value::Dict GetPasswordScriptFetcherInformation();

  // Retrieves the current state of the password script fetcher cache.
  base::Value::List GetPasswordScriptFetcherCache();

  // Gathers AutofillAssistant-related information, e.g. language, locale (can
  // be different from general Chrome settings)
  base::Value::Dict GetAutofillAssistantInformation() const;

  // Launches APC script on `url` with login `username`.
  void LaunchScript(const GURL& url, const std::string& username);

  // Removes finished requests from `pending_logins_requests_`.
  void OnLoginsRequestFinished(APCInternalsLoginsRequest* finished_request);

  // Parameters for starting an APC script as a debug run.
  absl::optional<ApcClient::DebugRunInformation> debug_run_information_;

  // Queue for pending requests fetching logins from password store.
  std::vector<std::unique_ptr<APCInternalsLoginsRequest>>
      pending_logins_requests_;

  // Profile password store.
  raw_ptr<password_manager::PasswordStoreInterface> profile_password_store_;

  // Represents all Gaia-account-scoped password stores.
  raw_ptr<password_manager::PasswordStoreInterface> account_password_store_;

  // A factory for weak pointers to the handler.
  base::WeakPtrFactory<APCInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_HANDLER_H_
