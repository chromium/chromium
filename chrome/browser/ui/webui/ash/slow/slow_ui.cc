// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/slow/slow_ui.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebUIMessageHandler;

namespace {

// JS API callbacks names.
const char kJsApiDisableTracing[] = "disableTracing";
const char kJsApiEnableTracing[] = "enableTracing";
const char kJsApiLoadComplete[] = "loadComplete";

}  // namespace

namespace ash {

void CreateAndAddSlowUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISlowHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"slowDisable", IDS_SLOW_DISABLE},
      {"slowEnable", IDS_SLOW_ENABLE},
      {"slowDescription", IDS_SLOW_DESCRIPTION},
      {"slowWarning", IDS_SLOW_WARNING},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddResourcePath("slow.js", IDR_SLOW_JS);
  source->AddResourcePath("slow.css", IDR_SLOW_CSS);
  source->SetDefaultResource(IDR_SLOW_HTML);
}

// The handler for Javascript messages related to the "slow" view.
class SlowHandler : public WebUIMessageHandler {
 public:
  explicit SlowHandler(Profile* profile);

  SlowHandler(const SlowHandler&) = delete;
  SlowHandler& operator=(const SlowHandler&) = delete;

  ~SlowHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void UpdatePage();

  // Handlers for JS WebUI messages.
  void HandleDisable(const base::Value::List& args);
  void HandleEnable(const base::Value::List& args);
  void LoadComplete(const base::Value::List& args);

  raw_ptr<Profile> profile_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
};

// SlowHandler ------------------------------------------------------------

SlowHandler::SlowHandler(Profile* profile) : profile_(profile) {
  user_pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  user_pref_registrar_->Init(profile_->GetPrefs());
}

SlowHandler::~SlowHandler() {}

void SlowHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kJsApiDisableTracing,
      base::BindRepeating(&SlowHandler::HandleDisable, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiEnableTracing,
      base::BindRepeating(&SlowHandler::HandleEnable, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiLoadComplete,
      base::BindRepeating(&SlowHandler::LoadComplete, base::Unretained(this)));
}

void SlowHandler::OnJavascriptAllowed() {
  user_pref_registrar_->Add(
      prefs::kPerformanceTracingEnabled,
      base::BindRepeating(&SlowHandler::UpdatePage, base::Unretained(this)));
}

void SlowHandler::OnJavascriptDisallowed() {
  user_pref_registrar_->RemoveAll();
}

void SlowHandler::HandleDisable(const base::Value::List& args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, false);
}

void SlowHandler::HandleEnable(const base::Value::List& args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, true);
}

void SlowHandler::LoadComplete(const base::Value::List& args) {
  AllowJavascript();
  UpdatePage();
}

void SlowHandler::UpdatePage() {
  PrefService* pref_service = profile_->GetPrefs();
  bool enabled = pref_service->GetBoolean(prefs::kPerformanceTracingEnabled);
  base::Value pref_value(enabled);
  FireWebUIListener("tracing-pref-changed", pref_value);
}

// SlowUI -----------------------------------------------------------------

SlowUI::SlowUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  web_ui->AddMessageHandler(std::make_unique<SlowHandler>(profile));

  // Set up the chrome://slow/ source.
  CreateAndAddSlowUIHTMLSource(profile);
}

}  // namespace ash
