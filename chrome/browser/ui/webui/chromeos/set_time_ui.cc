// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/set_time_ui.h"

#include <stdint.h>

#include <memory>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_screen.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

namespace {

class SetTimeMessageHandler : public content::WebUIMessageHandler,
                              public chromeos::SystemClockClient::Observer,
                              public system::TimezoneSettings::Observer {
 public:
  SetTimeMessageHandler() : weak_factory_(this) {}
  ~SetTimeMessageHandler() override = default;

  // WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "setTimePageReady",
        base::BindRepeating(&SetTimeMessageHandler::OnPageReady,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setTimeInSeconds",
        base::BindRepeating(&SetTimeMessageHandler::OnSetTime,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setTimezone",
        base::BindRepeating(&SetTimeMessageHandler::OnSetTimezone,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "doneClicked", base::BindRepeating(&SetTimeMessageHandler::DoneClicked,
                                           base::Unretained(this)));
  }

  void OnJavascriptAllowed() override {
    clock_observation_.Observe(SystemClockClient::Get());
    timezone_observation_.Observe(system::TimezoneSettings::GetInstance());
  }

  void OnJavascriptDisallowed() override {
    clock_observation_.Reset();
    timezone_observation_.Reset();
  }

 private:
  void OnPageReady(const base::ListValue* args) { AllowJavascript(); }

  // SystemClockClient::Observer:
  void SystemClockUpdated() override {
    FireWebUIListener("system-clock-updated");
  }

  // UI actually shows real device timezone, but only allows changing the user
  // timezone. If user timezone settings are different from system, this means
  // that user settings are overriden and must be disabled. (And we will still
  // show the actual device timezone.)
  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override {
    base::Value timezone_id(system::TimezoneSettings::GetTimezoneID(timezone));
    FireWebUIListener("system-timezone-changed", timezone_id);
  }

  // Handler for Javascript call to set the system clock when the user sets a
  // new time. Expects the time as the number of seconds since the Unix
  // epoch, treated as a double.
  void OnSetTime(const base::ListValue* args) {
    double seconds;
    if (!args->GetDouble(0, &seconds)) {
      NOTREACHED();
      return;
    }

    SystemClockClient::Get()->SetTime(static_cast<int64_t>(seconds));
  }

  // Handler for Javascript call to change the system time zone when the user
  // selects a new time zone. Expects the time zone ID as a string, as it
  // appears in the time zone option values.
  void OnSetTimezone(const base::ListValue* args) {
    std::string timezone_id;
    if (!args->GetString(0, &timezone_id)) {
      NOTREACHED();
      return;
    }

    Profile* profile = Profile::FromWebUI(web_ui());
    DCHECK(profile);
    system::SetTimezoneFromUI(profile, timezone_id);
  }

  void DoneClicked(const base::ListValue* args) {
    if (!parent_access::ParentAccessService::IsApprovalRequired(
            ash::SupervisedAction::kUpdateClock)) {
      OnParentAccessValidation(true);
      return;
    }

    double seconds;
    if (!args->GetDouble(0, &seconds)) {
      NOTREACHED();
      return;
    }

    AccountId account_id;
    bool is_user_logged_in = user_manager::UserManager::Get()->IsUserLoggedIn();
    if (is_user_logged_in) {
      account_id =
          user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
    }
    ash::ParentAccessController::Get()->ShowWidget(
        account_id,
        base::BindOnce(&SetTimeMessageHandler::OnParentAccessValidation,
                       weak_factory_.GetWeakPtr()),
        ash::SupervisedAction::kUpdateClock,
        !is_user_logged_in /* extra_dimmer */,
        base::Time::FromDoubleT(seconds));
  }

  void OnParentAccessValidation(bool success) {
    if (success)
      FireWebUIListener("validation-complete");
  }

  base::ScopedObservation<SystemClockClient, SystemClockClient::Observer>
      clock_observation_{this};
  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      timezone_observation_{this};
  base::WeakPtrFactory<SetTimeMessageHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SetTimeMessageHandler);
};

}  // namespace

SetTimeUI::SetTimeUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SetTimeMessageHandler>());

  // Set up the chrome://set-time source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISetTimeHost);
  webui::SetJSModuleDefaults(source);
  static constexpr webui::LocalizedString kStrings[] = {
      {"setTimeTitle", IDS_SET_TIME_TITLE},
      {"prompt", IDS_SET_TIME_PROMPT},
      {"timezoneLabel", IDS_SET_TIME_TIMEZONE_LABEL},
      {"dateLabel", IDS_SET_TIME_DATE_LABEL},
      {"timeLabel", IDS_SET_TIME_TIME_LABEL},
      {"doneButton", IDS_DONE},
  };
  source->AddLocalizedStrings(kStrings);

  base::DictionaryValue values;
  // List of list of strings: [[ID, name], [ID, name], ...]
  values.Set("timezoneList", chromeos::system::GetTimezoneList());

  // If we are not logged in, we need to show the time zone dropdown.
  values.SetBoolean("showTimezone", SetTimeDialog::ShouldShowTimezone());
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  values.SetString("currentTimezoneId", current_timezone_id);
  values.SetDouble("buildTime", base::GetBuildTime().ToJsTime());

  source->AddLocalizedStrings(values);

  source->AddResourcePath("set_time_browser_proxy.js",
                          IDR_SET_TIME_BROWSER_PROXY_JS);
  source->AddResourcePath("set_time_dialog.js", IDR_SET_TIME_DIALOG_JS);
  source->SetDefaultResource(IDR_SET_TIME_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

SetTimeUI::~SetTimeUI() = default;

}  // namespace chromeos
