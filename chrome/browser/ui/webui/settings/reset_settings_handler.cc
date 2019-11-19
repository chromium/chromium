// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif  // defined(OS_WIN)

namespace settings {

namespace {

reset_report::ChromeResetReport::ResetRequestOrigin
ResetRequestOriginFromString(const std::string& request_origin) {
  static const char kOriginUserClick[] = "userclick";
  static const char kOriginTriggeredReset[] = "triggeredreset";

  if (request_origin == ResetSettingsHandler::kCctResetSettingsHash)
    return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_CCT;
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

const char ResetSettingsHandler::kCctResetSettingsHash[] = "cct";

ResetSettingsHandler::ResetSettingsHandler(Profile* profile)
    : profile_(profile) {
  google_brand::GetBrand(&brandcode_);
}

ResetSettingsHandler::~ResetSettingsHandler() {}

ResetSettingsHandler* ResetSettingsHandler::Create(
    content::WebUIDataSource* html_source, Profile* profile) {
#if defined(OS_CHROMEOS)
  // TODO(crbug.com/891905): Centralize powerwash restriction checks.
  bool allow_powerwash = false;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  allow_powerwash =
      !connector->IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser() &&
      !user_manager::UserManager::Get()->IsLoggedInAsChildUser();
  html_source->AddBoolean("allowPowerwash", allow_powerwash);
#endif  // defined(OS_CHROMEOS)

  bool show_reset_profile_banner = false;
  static const int kBannerShowTimeInDays = 5;
  const base::Time then = chrome_prefs::GetResetTime(profile);
  if (!then.is_null()) {
    show_reset_profile_banner =
        (base::Time::Now() - then).InDays() < kBannerShowTimeInDays;
  }
  html_source->AddBoolean("showResetProfileBanner", show_reset_profile_banner);

  return new ResetSettingsHandler(profile);
}

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
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "onPowerwashDialogShow",
      base::BindRepeating(&ResetSettingsHandler::OnShowPowerwashDialog,
                          base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
}

void ResetSettingsHandler::HandleResetProfileSettings(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(3U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  bool send_settings = false;
  CHECK(args->GetBoolean(1, &send_settings));
  std::string request_origin_string;
  CHECK(args->GetString(2, &request_origin_string));
  reset_report::ChromeResetReport::ResetRequestOrigin request_origin =
      ResetRequestOriginFromString(request_origin_string);

  DCHECK(brandcode_.empty() || config_fetcher_);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(base::Bind(&ResetSettingsHandler::ResetProfile,
                                            base::Unretained(this), callback_id,
                                            send_settings, request_origin));
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
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  setting_snapshot_->RequestShortcuts(
      base::Bind(&ResetSettingsHandler::OnGetReportedSettingsDone,
                 callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void ResetSettingsHandler::OnGetReportedSettingsDone(std::string callback_id) {
  std::unique_ptr<base::ListValue> list =
      GetReadableFeedbackForSnapshot(profile_, *setting_snapshot_);
  ResolveJavascriptCallback(base::Value(callback_id), *list);
}

void ResetSettingsHandler::OnShowResetProfileDialog(
    const base::ListValue* args) {
  if (!GetResetter()->IsActive()) {
    setting_snapshot_ = std::make_unique<ResettableSettingsSnapshot>(profile_);
  }

  if (brandcode_.empty())
    return;
  config_fetcher_ = std::make_unique<BrandcodeConfigFetcher>(
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory(),
      base::Bind(&ResetSettingsHandler::OnSettingsFetched,
                 base::Unretained(this)),
      GURL("https://tools.google.com/service/update2"), brandcode_);
}

void ResetSettingsHandler::OnHideResetProfileDialog(
    const base::ListValue* args) {
  if (!GetResetter()->IsActive())
    setting_snapshot_.reset();
}

void ResetSettingsHandler::OnHideResetProfileBanner(
    const base::ListValue* args) {
  chrome_prefs::ClearResetTime(profile_);
}

void ResetSettingsHandler::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());
  // The master prefs is fetched. We are waiting for user pressing 'Reset'.
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
      base::Bind(&ResetSettingsHandler::OnResetProfileSettingsDone,
                 callback_weak_ptr_factory_.GetWeakPtr(), callback_id,
                 send_settings, request_origin));
  base::RecordAction(base::UserMetricsAction("ResetProfile"));
  UMA_HISTOGRAM_ENUMERATION(
      "ProfileReset.ResetRequestOrigin", request_origin,
      reset_report::ChromeResetReport::ResetRequestOrigin_MAX + 1);
}

ProfileResetter* ResetSettingsHandler::GetResetter() {
  if (!resetter_)
    resetter_ = std::make_unique<ProfileResetter>(profile_);
  return resetter_.get();
}

void ResetSettingsHandler::HandleGetTriggeredResetToolName(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  // Set up the localized strings for the triggered profile reset dialog.
  // Custom reset tool names are supported on Windows only.
  base::string16 reset_tool_name;
#if defined(OS_WIN)
  Profile* profile = Profile::FromWebUI(web_ui());
  TriggeredProfileResetter* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile);
  // TriggeredProfileResetter instance will be nullptr for incognito profiles.
  if (triggered_profile_resetter) {
    reset_tool_name = triggered_profile_resetter->GetResetToolName();

    // Now that a reset UI has been shown, don't trigger again for this profile.
    triggered_profile_resetter->ClearResetTrigger();
  }
#endif  // defined(OS_WIN)

  if (reset_tool_name.empty()) {
    reset_tool_name = l10n_util::GetStringUTF16(
        IDS_TRIGGERED_RESET_PROFILE_SETTINGS_DEFAULT_TOOL_NAME);
  }

  base::Value string_value(reset_tool_name);
  ResolveJavascriptCallback(*callback_id, string_value);
}

#if defined(OS_CHROMEOS)
void ResetSettingsHandler::OnShowPowerwashDialog(
     const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(
      "Reset.ChromeOS.PowerwashDialogShown",
      chromeos::reset::DIALOG_FROM_OPTIONS,
      chromeos::reset::DIALOG_VIEW_TYPE_SIZE);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
