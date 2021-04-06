// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using autofill::LogRouter;

namespace autofill {

content::WebUIDataSource* CreateInternalsHTMLSource(
    const std::string& source_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(source_name);
  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion, version_info::GetVersionNumber());
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    chrome::GetChannelName(chrome::WithExtendedStable(true)));
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, embedder_support::GetUserAgent());
  source->AddString("app_locale", g_browser_process->GetApplicationLocale());
  return source;
}

AutofillCacheResetter::AutofillCacheResetter(
    content::BrowserContext* browser_context)
    : remover_(
          content::BrowserContext::GetBrowsingDataRemover(browser_context)) {
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
}

void InternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void InternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void InternalsUIHandler::OnLoaded(const base::ListValue* args) {
  AllowJavascript();
  FireWebUIListener(call_on_load_, base::Value());
  // This is only available in contents, because the iOS BrowsingDataRemover
  // does not allow selectively deleting data per origin and we don't want to
  // wipe the entire cache.
  FireWebUIListener("enable-reset-cache-button", base::Value());
  FireWebUIListener(
      "notify-about-incognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
  FireWebUIListener("notify-about-variations",
                    *version_ui::GetVariationsList());
}

void InternalsUIHandler::OnResetCache(const base::ListValue* args) {
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

void InternalsUIHandler::StartSubscription() {
  LogRouter* log_router =
      get_log_router_function_.Run(Profile::FromWebUI(web_ui()));
  if (!log_router)
    return;

  registered_with_log_router_ = true;

  const auto& past_logs = log_router->RegisterReceiver(this);
  for (const auto& entry : past_logs)
    LogEntry(entry);
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

void InternalsUIHandler::LogEntry(const base::Value& entry) {
  if (!registered_with_log_router_ || entry.is_none())
    return;
  FireWebUIListener("add-raw-log", entry);
}

}  // namespace autofill
