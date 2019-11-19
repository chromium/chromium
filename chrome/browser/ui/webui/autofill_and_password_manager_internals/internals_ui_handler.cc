// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/grit/components_resources.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
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
  source->AddString(version_ui::kVersionModifier, chrome::GetChannelName());
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, GetUserAgent());
  source->AddString("app_locale", g_browser_process->GetApplicationLocale());
  return source;
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
}

void InternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void InternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void InternalsUIHandler::OnLoaded(const base::ListValue* args) {
  AllowJavascript();
  CallJavascriptFunction(call_on_load_);
  CallJavascriptFunction(
      "notifyAboutIncognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
  CallJavascriptFunction("notifyAboutVariations",
                         *version_ui::GetVariationsList());
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
  CallJavascriptFunction("addRawLog", entry);
}

}  // namespace autofill
