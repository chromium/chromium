// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif  // BUILDFLAG(IS_WIN)

namespace settings {

namespace {

reset_report::ChromeResetReport::ResetRequestOrigin
ResetRequestOriginFromString(const std::string& request_origin) {
  static const char kOriginUserClick[] = "userclick";
  static const char kOriginTriggeredReset[] = "triggeredreset";

  if (request_origin == kOriginUserClick)
    return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_USER_CLICK;
  if (request_origin == kOriginTriggeredReset) {
    return reset_report::ChromeResetReport::
        RESET_REQUEST_ORIGIN_TRIGGERED_RESET;
  }
  if (!request_origin.empty())
    NOTREACHED_IN_MIGRATION();

  return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_UNKNOWN;
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
const char ResetSettingsHandler::kCctResetSettingsHash[] = "cct";

// static
void ResetSettingsHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::settings::prefs::kSanitizeCompleted,
                                false);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
bool ResetSettingsHandler::ShouldShowResetProfileBanner(Profile* profile) {
  const base::Time reset_time = chrome_prefs::GetResetTime(profile);

  // If there is no reset time, do not show the banner.
  if (reset_time.is_null())
    return false;

  // Otherwise, only show the banner if it has been less than |kBannerShowTime|
  // since reset.
  static constexpr base::TimeDelta kBannerShowTime = base::Days(5);
  const base::TimeDelta since_reset = base::Time::Now() - reset_time;
  return since_reset < kBannerShowTime;
}

ResetSettingsHandler::ResetSettingsHandler(Profile* profile)
    : profile_(profile),
      resetter_(std::make_unique<ProfileResetter>(profile_)) {}

ResetSettingsHandler::~ResetSettingsHandler() {}

void ResetSettingsHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void ResetSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "performResetProfileSettings",
      base::BindRepeating(&ResetSettingsHandler::HandleResetProfileSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onShowResetProfileDialog",
      base::BindRepeating(&ResetSettingsHandler::OnShowResetProfileDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportedSettings",
      base::BindRepeating(&ResetSettingsHandler::HandleGetReportedSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onHideResetProfileDialog",
      base::BindRepeating(&ResetSettingsHandler::OnHideResetProfileDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onHideResetProfileBanner",
      base::BindRepeating(&ResetSettingsHandler::OnHideResetProfileBanner,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTriggeredResetToolName",
      base::BindRepeating(
          &ResetSettingsHandler::HandleGetTriggeredResetToolName,
          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "performSanitizeSettings",
      base::BindRepeating(&ResetSettingsHandler::SanitizeSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onShowSanitizeDialog",
      base::BindRepeating(&ResetSettingsHandler::OnShowSanitizeDialog,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ResetSettingsHandler::HandleResetProfileSettings(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(3U, args.size());
  const std::string& callback_id = args[0].GetString();
  const bool& send_settings = args[1].GetBool();
  std::string request_origin_string = args[2].GetString();
  reset_report::ChromeResetReport::ResetRequestOrigin request_origin =
      ResetRequestOriginFromString(request_origin_string);

  ResetProfile(callback_id, send_settings, request_origin);
}

void ResetSettingsHandler::OnResetProfileSettingsDone(
    std::string callback_id,
    bool send_feedback,
    reset_report::ChromeResetReport::ResetRequestOrigin request_origin) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
  if (send_feedback && setting_snapshot_) {
    ResettableSettingsSnapshot current_snapshot(profile_);
    int difference = setting_snapshot_->FindDifferentFields(current_snapshot);
    if (difference) {
      setting_snapshot_->Subtract(current_snapshot);
      std::unique_ptr<reset_report::ChromeResetReport> report_proto =
          SerializeSettingsReportToProto(*setting_snapshot_, difference);
      if (report_proto) {
        report_proto->set_reset_request_origin(request_origin);
        SendSettingsFeedbackProto(*report_proto, profile_);
      }
    }
  }
  setting_snapshot_.reset();
}

void ResetSettingsHandler::HandleGetReportedSettings(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  setting_snapshot_->RequestShortcuts(
      base::BindOnce(&ResetSettingsHandler::OnGetReportedSettingsDone,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void ResetSettingsHandler::OnGetReportedSettingsDone(std::string callback_id) {
  base::Value::List list =
      GetReadableFeedbackForSnapshot(profile_, *setting_snapshot_);
  ResolveJavascriptCallback(base::Value(callback_id), list);
}

void ResetSettingsHandler::OnShowResetProfileDialog(
    const base::Value::List& args) {
  if (!GetResetter()->IsActive()) {
    setting_snapshot_ = std::make_unique<ResettableSettingsSnapshot>(profile_);
  }
}

void ResetSettingsHandler::OnHideResetProfileDialog(
    const base::Value::List& args) {
  if (!GetResetter()->IsActive())
    setting_snapshot_.reset();
}

void ResetSettingsHandler::OnHideResetProfileBanner(
    const base::Value::List& args) {
  chrome_prefs::ClearResetTime(profile_);
}

void ResetSettingsHandler::ResetProfile(
    const std::string& callback_id,
    bool send_settings,
    reset_report::ChromeResetReport::ResetRequestOrigin request_origin) {
  GetResetter()->ResetSettings(
      ProfileResetter::PROFILE_RESETS, nullptr,
      base::BindOnce(&ResetSettingsHandler::OnResetProfileSettingsDone,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id,
                     send_settings, request_origin));

  base::RecordAction(base::UserMetricsAction("ResetProfile"));
}

ProfileResetter* ResetSettingsHandler::GetResetter() {
  if (!resetter_)
    resetter_ = std::make_unique<ProfileResetter>(profile_);
  return resetter_.get();
}

void ResetSettingsHandler::HandleGetTriggeredResetToolName(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  // Set up the localized strings for the triggered profile reset dialog.
  // Custom reset tool names are supported on Windows only.
  std::u16string reset_tool_name;
#if BUILDFLAG(IS_WIN)
  Profile* profile = Profile::FromWebUI(web_ui());
  TriggeredProfileResetter* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile);
  // TriggeredProfileResetter instance will be nullptr for incognito profiles.
  if (triggered_profile_resetter) {
    reset_tool_name = triggered_profile_resetter->GetResetToolName();

    // Now that a reset UI has been shown, don't trigger again for this profile.
    triggered_profile_resetter->ClearResetTrigger();
  }
#endif  // BUILDFLAG(IS_WIN)

  if (reset_tool_name.empty()) {
    reset_tool_name = l10n_util::GetStringUTF16(
        IDS_TRIGGERED_RESET_PROFILE_SETTINGS_DEFAULT_TOOL_NAME);
  }

  base::Value string_value(reset_tool_name);
  ResolveJavascriptCallback(callback_id, string_value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ResetSettingsHandler::OnShowSanitizeDialog(const base::Value::List& args) {
  // TODO(b/357057195) move sanitize functionality functions out of
  // ResetSettingsHandler and only leave the UI parts for ResetSettingsHandler.
  if (base::FeatureList::IsEnabled(ash::features::kSanitize)) {
    ash::SystemAppLaunchParams params;
    params.launch_source = apps::LaunchSource::kUnknown;
    ash::LaunchSystemWebAppAsync(ProfileManager::GetPrimaryUserProfile(),
                                 ash::SystemWebAppType::OS_SANITIZE, params);
  }
}

void ResetSettingsHandler::SanitizeSettings(const base::Value::List& args) {
  ProfileResetter::ResettableFlags to_sanitize =
      ProfileResetter::DEFAULT_SEARCH_ENGINE | ProfileResetter::HOMEPAGE |
      ProfileResetter::CONTENT_SETTINGS | ProfileResetter::EXTENSIONS |
      ProfileResetter::STARTUP_PAGES | ProfileResetter::PINNED_TABS |
      ProfileResetter::SHORTCUTS | ProfileResetter::NTP_CUSTOMIZATIONS |
      ProfileResetter::LANGUAGES | ProfileResetter::DNS_CONFIGURATIONS;
  // TODO(b/319446147): get send_feedback flag and pass it down
  GetResetter()->ResetSettings(
      to_sanitize, nullptr,
      base::BindOnce(&ResetSettingsHandler::OnSanitizeDone,
                     callback_weak_ptr_factory_.GetWeakPtr()));

  base::RecordAction(base::UserMetricsAction("Sanitize"));
}

void ResetSettingsHandler::OnSanitizeDone() {
  setting_snapshot_.reset();
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  prefs->SetBoolean(ash::settings::prefs::kSanitizeCompleted, true);
  prefs->CommitPendingWrite();
  chrome::AttemptRestart();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace settings
