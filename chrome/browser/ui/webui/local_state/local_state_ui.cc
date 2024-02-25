// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_state/local_state_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/local_state/local_state_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// UI Handler for chrome://local-state. Displays the Local State file as JSON.
class LocalStateUIHandler : public content::WebUIMessageHandler {
 public:
  LocalStateUIHandler() = default;

  LocalStateUIHandler(const LocalStateUIHandler&) = delete;
  LocalStateUIHandler& operator=(const LocalStateUIHandler&) = delete;

  ~LocalStateUIHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Called from JS when the page has loaded. Serializes local state prefs and
  // sends them to the page.
  void HandleRequestJson(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, the local state file contains some information about other
  // user accounts which we don't want to expose to other users. In that case,
  // this will filter out the prefs to only include variations and UMA related
  // fields, which don't contain PII.
  std::vector<std::string> accepted_pref_prefixes_{"variations",
                                                   "user_experience_metrics"};
#else
  std::vector<std::string> accepted_pref_prefixes_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

void LocalStateUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestJson",
      base::BindRepeating(&LocalStateUIHandler::HandleRequestJson,
                          base::Unretained(this)));
}

void LocalStateUIHandler::HandleRequestJson(const base::Value::List& args) {
  AllowJavascript();

  std::optional<std::string> json = local_state_utils::GetPrefsAsJson(
      g_browser_process->local_state(), accepted_pref_prefixes_);
  if (!json) {
    json = "Error loading Local State file.";
  }

  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, base::Value(*json));
}

}  // namespace

LocalStateUI::LocalStateUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://local-state source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUILocalStateHost);
  html_source->SetDefaultResource(IDR_LOCAL_STATE_HTML);
  html_source->AddResourcePath("local_state.js", IDR_LOCAL_STATE_JS);
  web_ui->AddMessageHandler(std::make_unique<LocalStateUIHandler>());
}

LocalStateUI::~LocalStateUI() {
}
