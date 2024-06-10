// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace autofill {
class LogRouter;
}

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace autofill {

constexpr char kCacheResetDone[] =
    "Done. Please close and reopen all tabs that should be affected by the "
    "cache reset.";
constexpr char kCacheResetAlreadyInProgress[] = "Reset already in progress";

void CreateAndAddInternalsHTMLSource(Profile* profile,
                                     const std::string& source_name);

// Class that wipes responses from the Autofill server from the HTTP cache.
class AutofillCacheResetter : public content::BrowsingDataRemover::Observer {
 public:
  using Callback = base::OnceCallback<void(const std::string&)>;

  explicit AutofillCacheResetter(content::BrowserContext* browser_context);
  ~AutofillCacheResetter() override;
  AutofillCacheResetter(const AutofillCacheResetter&) = delete;
  AutofillCacheResetter operator=(const AutofillCacheResetter) = delete;

  void ResetCache(Callback callback);

 private:
  // Implements content::BrowsingDataRemover::Observer.
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;
  raw_ptr<content::BrowsingDataRemover> remover_;
  Callback callback_;
};

// UI handler for chrome://password-manager-internals and
// chrome://autofill-internals that takes care of subscribing to the autofill
// logging instance.
class InternalsUIHandler : public content::WebUIMessageHandler,
                           public autofill::LogReceiver {
 public:
  using GetLogRouterFunction =
      base::RepeatingCallback<autofill::LogRouter*(content::BrowserContext*)>;

  InternalsUIHandler(std::string call_on_load,
                     GetLogRouterFunction get_log_router_function);

  InternalsUIHandler(const InternalsUIHandler&) = delete;
  InternalsUIHandler& operator=(const InternalsUIHandler&) = delete;

  ~InternalsUIHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Implements content::WebUIMessageHandler.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // LogReceiver implementation.
  void LogEntry(const base::Value::Dict& entry) override;

  void StartSubscription();
  void EndSubscription();

  // JavaScript call handler.
  void OnLoaded(const base::Value::List& args);
  void OnResetCache(const base::Value::List& args);
  void OnResetUpmEviction(const base::Value::List& args);
  void OnResetAccountStorageNotice(const base::Value::List& args);

  void OnResetCacheDone(const std::string& message);

  // JavaScript function to be called on load.
  std::string call_on_load_;
  GetLogRouterFunction get_log_router_function_;

  // Whether |this| is registered as a log receiver with the LogRouter.
  bool registered_with_log_router_ = false;

  std::optional<AutofillCacheResetter> autofill_cache_resetter_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
