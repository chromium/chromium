// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/slow_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
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

// Page JS API function names.
const char kJsApiTracingPrefChanged[] = "options.Slow.tracingPrefChanged";

}  // namespace

namespace chromeos {

content::WebUIDataSource* CreateSlowUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISlowHost);

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
  return source;
}

// The handler for Javascript messages related to the "slow" view.
class SlowHandler : public WebUIMessageHandler {
 public:
  explicit SlowHandler(Profile* profile);
  ~SlowHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void UpdatePage();

  // Handlers for JS WebUI messages.
  void HandleDisable(const base::ListValue* args);
  void HandleEnable(const base::ListValue* args);
  void LoadComplete(const base::ListValue* args);

  Profile* profile_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SlowHandler);
};

// SlowHandler ------------------------------------------------------------

SlowHandler::SlowHandler(Profile* profile) : profile_(profile) {
}

SlowHandler::~SlowHandler() {
}

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

  user_pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  user_pref_registrar_->Init(profile_->GetPrefs());
  user_pref_registrar_->Add(
      prefs::kPerformanceTracingEnabled,
      base::BindRepeating(&SlowHandler::UpdatePage, base::Unretained(this)));
}

void SlowHandler::HandleDisable(const base::ListValue* args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, false);
}

void SlowHandler::HandleEnable(const base::ListValue* args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, true);
}

void SlowHandler::LoadComplete(const base::ListValue* args) {
  UpdatePage();
}

void SlowHandler::UpdatePage() {
  PrefService* pref_service = profile_->GetPrefs();
  bool enabled = pref_service->GetBoolean(prefs::kPerformanceTracingEnabled);
  base::Value pref_value(enabled);
  web_ui()->CallJavascriptFunctionUnsafe(kJsApiTracingPrefChanged, pref_value);
}

// SlowUI -----------------------------------------------------------------

SlowUI::SlowUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  web_ui->AddMessageHandler(std::make_unique<SlowHandler>(profile));

  // Set up the chrome://slow/ source.
  content::WebUIDataSource::Add(profile, CreateSlowUIHTMLSource());
}

}  // namespace chromeos

