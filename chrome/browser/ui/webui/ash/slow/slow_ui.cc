// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/slow/slow_ui.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/webui_url_constants.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/slow_resources.h"
#include "chrome/grit/slow_resources_map.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

using content::WebUIMessageHandler;

namespace {

// JS API callbacks names.
const char kJsApiDisableTracing[] = "disableTracing";
const char kJsApiEnableTracing[] = "enableTracing";
const char kJsApiLoadComplete[] = "loadComplete";

}  // namespace

namespace ash {

void CreateAndAddSlowUIHTMLSource(content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, ash::kChromeUISlowHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"slowDisable", IDS_SLOW_DISABLE},
      {"slowEnable", IDS_SLOW_ENABLE},
      {"slowDescription", IDS_SLOW_DESCRIPTION},
      {"slowWarning", IDS_SLOW_WARNING},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddResourcePaths(kSlowResources);
  source->SetDefaultResource(IDR_SLOW_SLOW_HTML);
}

// The handler for Javascript messages related to the "slow" view.
class SlowHandler : public WebUIMessageHandler {
 public:
  explicit SlowHandler(PrefService* pref_service);

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
  void HandleDisable(const base::ListValue& args);
  void HandleEnable(const base::ListValue& args);
  void LoadComplete(const base::ListValue& args);

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
};

// SlowHandler ------------------------------------------------------------

SlowHandler::SlowHandler(PrefService* pref_service)
    : pref_service_(pref_service) {
  user_pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  user_pref_registrar_->Init(pref_service_);
}

SlowHandler::~SlowHandler() = default;

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
      ash::prefs::kPerformanceTracingEnabled,
      base::BindRepeating(&SlowHandler::UpdatePage, base::Unretained(this)));
}

void SlowHandler::OnJavascriptDisallowed() {
  user_pref_registrar_->RemoveAll();
}

void SlowHandler::HandleDisable(const base::ListValue& args) {
  pref_service_->SetBoolean(ash::prefs::kPerformanceTracingEnabled, false);
}

void SlowHandler::HandleEnable(const base::ListValue& args) {
  pref_service_->SetBoolean(ash::prefs::kPerformanceTracingEnabled, true);
}

void SlowHandler::LoadComplete(const base::ListValue& args) {
  AllowJavascript();
  UpdatePage();
}

void SlowHandler::UpdatePage() {
  bool enabled =
      pref_service_->GetBoolean(ash::prefs::kPerformanceTracingEnabled);
  base::Value pref_value(enabled);
  FireWebUIListener("tracing-pref-changed", pref_value);
}

// SlowUI -----------------------------------------------------------------

SlowUI::SlowUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::BrowserContext* const browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  web_ui->AddMessageHandler(std::make_unique<SlowHandler>(
      user_prefs::UserPrefs::Get(browser_context)));

  // Set up the chrome://slow/ source.
  CreateAndAddSlowUIHTMLSource(browser_context);
}

}  // namespace ash
