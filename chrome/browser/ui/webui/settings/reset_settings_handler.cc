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
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/pref_names.h"
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
    NOTREACHED();

  return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_UNKNOWN;
}

}  // namespace

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
    : profile_(profile) {
  google_brand::GetBrand(&brandcode_);
}

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

  DCHECK(brandcode_.empty() || config_fetcher_);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(base::BindOnce(
        &ResetSettingsHandler::ResetProfile, base::Unretained(this),
        callback_id, send_settings, request_origin));
  } else {
    ResetProfile(callback_id, send_settings, request_origin);
  }
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

  if (brandcode_.empty())
    return;
  config_fetcher_ = std::make_unique<BrandcodeConfigFetcher>(
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory(),
      base::BindOnce(&ResetSettingsHandler::OnSettingsFetched,
                     base::Unretained(this)),
      GURL("https://tools.google.com/service/update2"), brandcode_);
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

void ResetSettingsHandler::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());
  // The initial prefs is fetched. We are waiting for user pressing 'Reset'.
}

void ResetSettingsHandler::ResetProfile(
    const std::string& callback_id,
    bool send_settings,
    reset_report::ChromeResetReport::ResetRequestOrigin request_origin) {
  CHECK(!GetResetter()->IsActive());

  std::unique_ptr<BrandcodedDefaultSettings> default_settings;
  if (config_fetcher_) {
    DCHECK(!config_fetcher_->IsActive());
    default_settings = config_fetcher_->GetSettings();
    config_fetcher_.reset();
  } else {
    DCHECK(brandcode_.empty());
  }

  // If failed to fetch BrandcodedDefaultSettings or this is an organic
  // installation, use default settings.
  if (!default_settings)
    default_settings = std::make_unique<BrandcodedDefaultSettings>();

  GetResetter()->Reset(
      ProfileResetter::ALL, std::move(default_settings),
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

}  // namespace settings
