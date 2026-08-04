// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#include "components/device_reauth/device_authenticator.h"
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

inline constexpr char kCacheResetDone[] =
    "Done. Please close and reopen all tabs that should be affected by the "
    "cache reset.";
inline constexpr char kCacheResetAlreadyInProgress[] =
    "Reset already in progress";

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
class InternalsUIHandler
    : public content::WebUIMessageHandler,
      public LogReceiver,
      public EntityDataManager::Observer,
      public AutofillAiPersonalContextAccessManager::Observer {
 public:
  using GetLogRouterFunction =
      base::RepeatingCallback<LogRouter*(content::BrowserContext*)>;

  InternalsUIHandler(std::string call_on_load,
                     base::Value call_on_load_argument,
                     GetLogRouterFunction get_log_router_function);

  InternalsUIHandler(const InternalsUIHandler&) = delete;
  InternalsUIHandler& operator=(const InternalsUIHandler&) = delete;

  ~InternalsUIHandler() override;

  void set_authenticator_for_testing(
      std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator) {
    authenticator_ = std::move(authenticator);
  }

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Implements content::WebUIMessageHandler.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // LogReceiver implementation.
  void LogEntry(const base::DictValue& entry) override;

  void StartSubscription();
  void EndSubscription();

  // EntityDataManager::Observer:
  void OnEntityInstancesChanged() override;

  // AutofillAiPersonalContextAccessManager::Observer:
  void OnPrefetchContextComplete(
      const AutofillAiPersonalContextAccessManager& manager,
      std::optional<base::span<const EntityInstance>> entities) override;

  void SendAutofillAiEntitiesToWebUI();
  void FetchNextPersonalContextType();

  // JavaScript call handler.
  void OnDeleteAutofillAiCacheEntry(const base::ListValue& args);
  void OnGetAutofillAiCache(const base::ListValue& args);
  void OnGetAutofillAiEntities(const base::ListValue& args);
  void OnAuthenticateToRevealMaskedEntities(const base::ListValue& args);
  void OnReauthCompleted(bool auth_succeeded);
  void OnLoaded(const base::ListValue& args);
  void OnResetCache(const base::ListValue& args);
  void OnDumpAddresses(const base::ListValue& args);
  void OnSetPasswordChangeOverrideUrl(const base::ListValue& args);
  void CheckAtMemoryPermissions(const base::ListValue& args);
#if !BUILDFLAG(IS_ANDROID)
  void CheckAutofillAiPermissions(const base::ListValue& args);
  void SetDomNodeId(const base::ListValue& args);
#endif

  void OnResetCacheDone(const std::string& message);

  // JavaScript function to be called on load.
  std::string call_on_load_;
  // The argument to be passed to the on load function.
  base::Value call_on_load_argument_;
  GetLogRouterFunction get_log_router_function_;

  // Whether |this| is registered as a log receiver with the LogRouter.
  bool registered_with_log_router_ = false;

  std::deque<EntityType> pending_prefetch_types_;
  std::optional<EntityType> current_prefetch_type_;
  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      entity_data_observation_{this};
  base::ScopedObservation<AutofillAiPersonalContextAccessManager,
                          AutofillAiPersonalContextAccessManager::Observer>
      pcontext_observation_{this};

  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;
  std::optional<AutofillCacheResetter> autofill_cache_resetter_;

  base::WeakPtrFactory<InternalsUIHandler> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
