// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#endif

using autofill::LogRouter;

namespace autofill {

void CreateAndAddInternalsHTMLSource(Profile* profile,
                                     const std::string& source_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, source_name);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion,
                    std::string(version_info::GetVersionNumber()));
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    chrome::GetChannelName(chrome::WithExtendedStable(true)));
  source->AddString(version_ui::kCL,
                    std::string(version_info::GetLastChange()));
  source->AddString(version_ui::kUserAgent, embedder_support::GetUserAgent());
  source->AddString("app_locale", g_browser_process->GetApplicationLocale());
}

AutofillCacheResetter::AutofillCacheResetter(
    content::BrowserContext* browser_context)
    : remover_(browser_context->GetBrowsingDataRemover()) {
  remover_->AddObserver(this);
}

AutofillCacheResetter::~AutofillCacheResetter() {
  remover_->RemoveObserver(this);
}

void AutofillCacheResetter::ResetCache(Callback callback) {
  if (callback_) {
    std::move(callback).Run(kCacheResetAlreadyInProgress);
    return;
  }

  callback_ = std::move(callback);

  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(
      url::Origin::Create(GURL("https://content-autofill.googleapis.com")));
  remover_->RemoveWithFilterAndReply(
      base::Time::Min(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      std::move(filter_builder), this);
}

void AutofillCacheResetter::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  std::move(callback_).Run(kCacheResetDone);
}

InternalsUIHandler::InternalsUIHandler(
    std::string call_on_load,
    GetLogRouterFunction get_log_router_function)
    : call_on_load_(std::move(call_on_load)),
      get_log_router_function_(std::move(get_log_router_function)) {}

InternalsUIHandler::~InternalsUIHandler() {
  EndSubscription();
}

void InternalsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&InternalsUIHandler::OnLoaded,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetCache", base::BindRepeating(&InternalsUIHandler::OnResetCache,
                                        base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "resetUpmEviction",
      base::BindRepeating(&InternalsUIHandler::OnResetUpmEviction,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetAccountStorageNotice",
      base::BindRepeating(&InternalsUIHandler::OnResetAccountStorageNotice,
                          base::Unretained(this)));
#endif
}

void InternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void InternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void InternalsUIHandler::OnLoaded(const base::Value::List& args) {
  AllowJavascript();
  FireWebUIListener(call_on_load_, base::Value());
  // This is only available in contents, because the iOS BrowsingDataRemover
  // does not allow selectively deleting data per origin and we don't want to
  // wipe the entire cache.
  FireWebUIListener("enable-reset-cache-button", base::Value());
  FireWebUIListener(
      "notify-about-incognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
  FireWebUIListener("notify-about-variations", version_ui::GetVariationsList());

#if BUILDFLAG(IS_ANDROID)
  auto* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  FireWebUIListener("enable-reset-upm-eviction-button",
                    password_manager_upm_eviction::IsCurrentUserEvicted(prefs));
#endif
}

void InternalsUIHandler::OnResetCache(const base::Value::List& args) {
  if (!autofill_cache_resetter_) {
    content::BrowserContext* browser_context = Profile::FromWebUI(web_ui());
    autofill_cache_resetter_.emplace(browser_context);
  }
  autofill_cache_resetter_->ResetCache(base::BindOnce(
      &InternalsUIHandler::OnResetCacheDone, base::Unretained(this)));
}

void InternalsUIHandler::OnResetCacheDone(const std::string& message) {
  FireWebUIListener("notify-reset-done", base::Value(message));
}

#if BUILDFLAG(IS_ANDROID)
void InternalsUIHandler::OnResetUpmEviction(const base::Value::List& args) {
  auto* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  bool is_user_unenrolled =
      password_manager_upm_eviction::IsCurrentUserEvicted(prefs);
  if (is_user_unenrolled) {
    prefs->ClearPref(password_manager::prefs::
                         kUnenrolledFromGoogleMobileServicesDueToErrors);
  } else {
    prefs->SetBoolean(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        true);
    prefs->SetInteger(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        0);
    prefs->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt, 0.0);
  }
  FireWebUIListener("enable-reset-upm-eviction-button",
                    base::Value(!is_user_unenrolled));
}

void InternalsUIHandler::OnResetAccountStorageNotice(
    const base::Value::List& args) {
  Profile::FromWebUI(web_ui())->GetPrefs()->ClearPref(
      password_manager::prefs::kAccountStorageNoticeShown);
}
#endif

void InternalsUIHandler::StartSubscription() {
  LogRouter* log_router =
      get_log_router_function_.Run(Profile::FromWebUI(web_ui()));
  if (!log_router)
    return;

  registered_with_log_router_ = true;
  log_router->RegisterReceiver(this);
}

void InternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_)
    return;
  registered_with_log_router_ = false;
  LogRouter* log_router =
      get_log_router_function_.Run(Profile::FromWebUI(web_ui()));
  if (log_router)
    log_router->UnregisterReceiver(this);
}

void InternalsUIHandler::LogEntry(const base::Value::Dict& entry) {
  if (!registered_with_log_router_)
    return;
  FireWebUIListener("add-structured-log", entry);
}

}  // namespace autofill
