// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_state/local_state_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// On ChromeOS, the local state file contains some information about other
// user accounts which we don't want to expose to other users. Use an allowlist
// to only show variations and UMA related fields which don't contain PII.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define ENABLE_FILTERING true
#else
#define ENABLE_FILTERING false
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// UI Handler for chrome://local-state. Displays the Local State file as JSON.
class LocalStateUIHandler : public content::WebUIMessageHandler {
 public:
  LocalStateUIHandler();
  ~LocalStateUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Called from JS when the page has loaded. Serializes local state prefs and
  // sends them to the page.
  void HandleRequestJson(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LocalStateUIHandler);
};

LocalStateUIHandler::LocalStateUIHandler() {
}

LocalStateUIHandler::~LocalStateUIHandler() {
}

void LocalStateUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestJson",
      base::BindRepeating(&LocalStateUIHandler::HandleRequestJson,
                          base::Unretained(this)));
}

void LocalStateUIHandler::HandleRequestJson(const base::ListValue* args) {
  AllowJavascript();
  base::Value local_state_values =
      g_browser_process->local_state()->GetPreferenceValues(
          PrefService::EXCLUDE_DEFAULTS);
  if (ENABLE_FILTERING) {
    std::vector<std::string> allowlisted_prefixes = {"variations",
                                                     "user_experience_metrics"};
    internal::FilterPrefs(allowlisted_prefixes, local_state_values);
  }
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  bool result = serializer.Serialize(local_state_values);
  if (!result)
    json = "Error loading Local State file.";

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, base::Value(json));
}

// Returns true if |pref_name| starts with one of the |valid_prefixes|.
bool HasValidPrefix(const std::string& pref_name,
                    const std::vector<std::string> valid_prefixes) {
  for (const std::string& prefix : valid_prefixes) {
    if (base::StartsWith(pref_name, prefix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

}  // namespace

namespace internal {

void FilterPrefs(const std::vector<std::string>& valid_prefixes,
                 base::Value& prefs) {
  std::vector<std::string> prefs_to_remove;
  for (const auto& it : prefs.DictItems()) {
    if (!HasValidPrefix(it.first, valid_prefixes))
      prefs_to_remove.push_back(it.first);
  }
  for (const std::string& pref_to_remove : prefs_to_remove) {
    bool successfully_removed = prefs.RemovePath(pref_to_remove);
    DCHECK(successfully_removed);
  }
}

}  // namespace internal

LocalStateUI::LocalStateUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://local-state source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUILocalStateHost);
  html_source->SetDefaultResource(IDR_LOCAL_STATE_HTML);
  html_source->AddResourcePath("local_state.js", IDR_LOCAL_STATE_JS);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
  web_ui->AddMessageHandler(std::make_unique<LocalStateUIHandler>());
}

LocalStateUI::~LocalStateUI() {
}
