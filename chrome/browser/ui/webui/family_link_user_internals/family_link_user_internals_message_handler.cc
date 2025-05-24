// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_message_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

using content::BrowserThread;

// Creates a 'section' for display on about:family-link-user-internals,
// consisting of a title and a list of fields. Returns a pointer to the new
// section's contents, for use with |AddSectionEntry| below. Note that
// |parent_list|, not the caller, owns the newly added section.
base::Value::List* AddSection(base::Value::List* parent_list,
                              std::string_view title) {
  base::Value::Dict section;
  base::Value::List section_contents;
  section.Set("title", title);
  // Grab a raw pointer to the result before |Pass()|ing it on.
  base::Value::List* result =
      section.Set("data", std::move(section_contents))->GetIfList();
  parent_list->Append(std::move(section));
  return result;
}

// Adds a bool entry to a section (created with |AddSection| above).
void AddSectionEntry(base::Value::List* section_list,
                     std::string_view name,
                     bool value) {
  base::Value::Dict entry;
  entry.Set("stat_name", name);
  entry.Set("stat_value", value);
  entry.Set("is_valid", true);
  section_list->Append(std::move(entry));
}

// Adds a string entry to a section (created with |AddSection| above).
void AddSectionEntry(base::Value::List* section_list,
                     std::string_view name,
                     std::string_view value) {
  base::Value::Dict entry;
  entry.Set("stat_name", name);
  entry.Set("stat_value", value);
  entry.Set("is_valid", true);
  section_list->Append(std::move(entry));
}

std::string FilteringBehaviorToString(
    supervised_user::FilteringBehavior behavior) {
  switch (behavior) {
    case supervised_user::FilteringBehavior::kAllow:
      return "Allow";
    case supervised_user::FilteringBehavior::kBlock:
      return "Block";
    case supervised_user::FilteringBehavior::kInvalid:
      return "Invalid";
  }
  return "Unknown";
}

std::string WebFilterTypeToString(
    supervised_user::WebFilterType web_filter_type) {
  switch (web_filter_type) {
    case supervised_user::WebFilterType::kAllowAllSites:
      return "Allow all sites";
    case supervised_user::WebFilterType::kTryToBlockMatureSites:
      return "Try to block mature sites";
    case supervised_user::WebFilterType::kCertainSites:
      return "Only certain sites";
    case supervised_user::WebFilterType::kDisabled:
      return "Disabled";
    case supervised_user::WebFilterType::kMixed:
      NOTREACHED()
          << "That value is not intended to be set, but is rather "
             "used to indicate multiple settings used in profiles in metrics.";
  }
  return "Unknown";
}

std::string FilteringResultToString(
    supervised_user::SupervisedUserURLFilter::Result result) {
  std::string return_value = FilteringBehaviorToString(result.behavior);
  if (!result.IsClassificationSuccessful()) {
    return_value += " (Uncertain)";
  }
  return return_value;
}

std::string FilteringReasonToString(
    supervised_user::SupervisedUserURLFilter::Result result) {
  switch (result.reason) {
    case supervised_user::FilteringBehaviorReason::DEFAULT:
      return "Default";
    case supervised_user::FilteringBehaviorReason::ASYNC_CHECKER:
      CHECK(result.async_check_details.has_value())
          << "reason == ASYNC_CHECKER must imply async_check_details";
      if (result.async_check_details->reason ==
          safe_search_api::ClassificationDetails::Reason::kCachedResponse) {
        return "AsyncChecker (Cached)";
      }
      return "AsyncChecker";
    case supervised_user::FilteringBehaviorReason::MANUAL:
      return "Manual";
    case supervised_user::FilteringBehaviorReason::FILTER_DISABLED:
      return "Filtering is disabled";
  }
  NOTREACHED();
}

bool IsSubjectToFamilyLinkParentalControls(
    const signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return false;
  }
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return account_info.capabilities.is_subject_to_parental_controls() ==
         signin::Tribool::kTrue;
}

std::string WebContentFiltersToToggle(
    FamilyLinkUserInternalsMessageHandler::WebContentFilters value) {
  switch (value) {
    case FamilyLinkUserInternalsMessageHandler::WebContentFilters::kDisabled:
      return "off";
    case FamilyLinkUserInternalsMessageHandler::WebContentFilters::kEnabled:
      return "on";
    default:
      NOTREACHED();
  }
}
FamilyLinkUserInternalsMessageHandler::WebContentFilters
ToggleToWebContentFilters(std::string value) {
  if (value == "on") {
    return FamilyLinkUserInternalsMessageHandler::WebContentFilters::kEnabled;
  }
  if (value == "off") {
    return FamilyLinkUserInternalsMessageHandler::WebContentFilters::kDisabled;
  }
  NOTREACHED();
}

}  // namespace

FamilyLinkUserInternalsMessageHandler::FamilyLinkUserInternalsMessageHandler() =
    default;

FamilyLinkUserInternalsMessageHandler::
    ~FamilyLinkUserInternalsMessageHandler() = default;

void FamilyLinkUserInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "registerForEvents",
      base::BindRepeating(
          &FamilyLinkUserInternalsMessageHandler::HandleRegisterForEvents,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getBasicInfo",
      base::BindRepeating(
          &FamilyLinkUserInternalsMessageHandler::HandleGetBasicInfo,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "tryURL",
      base::BindRepeating(&FamilyLinkUserInternalsMessageHandler::HandleTryURL,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changeSearchContentFilters",
      base::BindRepeating(&FamilyLinkUserInternalsMessageHandler::
                              HandleChangeSearchContentFilters,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changeBrowserContentFilters",
      base::BindRepeating(&FamilyLinkUserInternalsMessageHandler::
                              HandleChangeBrowserContentFilters,
                          base::Unretained(this)));
}

void FamilyLinkUserInternalsMessageHandler::OnJavascriptDisallowed() {
  url_filter_observation_.Reset();
  identity_manager_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void FamilyLinkUserInternalsMessageHandler::OnURLFilterChanged() {
  SendBasicInfo();
}

void FamilyLinkUserInternalsMessageHandler::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  OnAccountChanged();
}
void FamilyLinkUserInternalsMessageHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  OnAccountChanged();
}
void FamilyLinkUserInternalsMessageHandler::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  OnAccountChanged();
}
void FamilyLinkUserInternalsMessageHandler::
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error,
        signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  OnAccountChanged();
}
void FamilyLinkUserInternalsMessageHandler::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  OnAccountChanged();
}

void FamilyLinkUserInternalsMessageHandler::OnAccountChanged() {
  // There's no need to change the search_content_filtering_status_ setting even
  // if the account changes to family link supervised - Chrome will use
  // feature's default.
  SendWebContentFiltersInfo();
  SendBasicInfo();
}

supervised_user::SupervisedUserService*
FamilyLinkUserInternalsMessageHandler::GetSupervisedUserService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return SupervisedUserServiceFactory::GetForProfile(
      profile->GetOriginalProfile());
}

void FamilyLinkUserInternalsMessageHandler::HandleRegisterForEvents(
    const base::Value::List& args) {
  CHECK(args.empty()) << "Expected call is (void)";

  AllowJavascript();
  if (!url_filter_observation_.IsObserving()) {
    url_filter_observation_.Observe(GetSupervisedUserService()->GetURLFilter());
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager_observation_.IsObserving() && identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
    SendWebContentFiltersInfo();
  }
}

void FamilyLinkUserInternalsMessageHandler::HandleGetBasicInfo(
    const base::Value::List& args) {
  SendBasicInfo();
}

void FamilyLinkUserInternalsMessageHandler::HandleTryURL(
    const base::Value::List& args) {
  CHECK(args.size() == 2u && args[0].is_string() && args[1].is_string())
      << "Expected call is (callback_id: string, url_str: string)";

  const std::string& callback_id = args[0].GetString();
  const std::string& url_str = args[1].GetString();

  GURL url = url_formatter::FixupURL(url_str, std::string());
  if (!url.is_valid()) {
    return;
  }

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  content::WebContents* web_contents =
      web_ui() ? web_ui()->GetWebContents() : nullptr;
  bool skip_manual_parent_filter = false;

  if (web_contents) {
    skip_manual_parent_filter =
        supervised_user::ShouldContentSkipParentAllowlistFiltering(
            web_contents->GetOutermostWebContents());
  }

  filter->GetFilteringBehaviorWithAsyncChecks(
      url,
      base::BindOnce(&FamilyLinkUserInternalsMessageHandler::OnTryURLResult,
                     weak_factory_.GetWeakPtr(), callback_id),
      skip_manual_parent_filter);
}

void FamilyLinkUserInternalsMessageHandler::ConfigureSearchContentFilters() {
  Profile* profile = Profile::FromWebUI(web_ui());
  switch (search_content_filtering_status_) {
    case WebContentFilters::kDisabled:
      supervised_user::DisableSearchContentFilters(*profile->GetPrefs());
      break;
    case WebContentFilters::kEnabled:
      supervised_user::EnableSearchContentFilters(*profile->GetPrefs());
      break;
    default:
      NOTREACHED();
  }
}
void FamilyLinkUserInternalsMessageHandler::ConfigureBrowserContentFilters() {
  Profile* profile = Profile::FromWebUI(web_ui());
  switch (browser_content_filtering_status_) {
    case WebContentFilters::kDisabled:
      supervised_user::DisableBrowserContentFilters(*profile->GetPrefs());
      break;
    case WebContentFilters::kEnabled:
      supervised_user::EnableBrowserContentFilters(*profile->GetPrefs());
      break;
    default:
      NOTREACHED();
  }
}

void FamilyLinkUserInternalsMessageHandler::HandleChangeSearchContentFilters(
    const base::Value::List& args) {
  CHECK(args.size() == 1u && args[0].is_string())
      << "Expected call is (toggle_status: string)";

  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSubjectToFamilyLinkParentalControls(
          IdentityManagerFactory::GetForProfile(profile))) {
    // Feature not available for the Family Link supervised users.
    return;
  }

  const std::string& toggle_status = args[0].GetString();
  search_content_filtering_status_ = ToggleToWebContentFilters(toggle_status);
  ConfigureSearchContentFilters();
}

void FamilyLinkUserInternalsMessageHandler::HandleChangeBrowserContentFilters(
    const base::Value::List& args) {
  CHECK(args.size() == 1u && args[0].is_string())
      << "Expected call is (toggle_status: string)";

  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSubjectToFamilyLinkParentalControls(
          IdentityManagerFactory::GetForProfile(profile))) {
    // Feature not available for the Family Link supervised users.
    return;
  }

  const std::string& toggle_status = args[0].GetString();
  browser_content_filtering_status_ = ToggleToWebContentFilters(toggle_status);
  ConfigureBrowserContentFilters();
}

void FamilyLinkUserInternalsMessageHandler::SendBasicInfo() {
  base::Value::List section_list;
  Profile* profile = Profile::FromWebUI(web_ui());

  base::Value::List* section_profile = AddSection(&section_list, "Profile");
  AddSectionEntry(section_profile, "Account", profile->GetProfileUserName());

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();

  base::Value::List* section_filter = AddSection(&section_list, "Filter");
  AddSectionEntry(section_filter, "SafeSites enabled",
                  supervised_user::IsSafeSitesEnabled(*profile->GetPrefs()));
  AddSectionEntry(section_filter, "Web filter type",
                  WebFilterTypeToString(filter->GetWebFilterType()));

  base::Value::List* section_search =
      AddSection(&section_list, "Google search");
  AddSectionEntry(
      section_search, "Safe search enforced",
      supervised_user::IsGoogleSafeSearchEnforced(*profile->GetPrefs()));

  base::Value::List* section_browser =
      AddSection(&section_list, "Browser and web");
  AddSectionEntry(section_browser, "Incognito mode allowed",
                  IncognitoModePrefs::IsIncognitoAllowed(profile));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // |identity_manager| is null in incognito and guest profiles.
  if (identity_manager) {
    for (const auto& account :
         identity_manager
             ->GetExtendedAccountInfoForAccountsWithRefreshToken()) {
      base::Value::List* section_user = AddSection(
          &section_list, "User Information for " + account.full_name);
      AddSectionEntry(section_user, "Account id",
                      account.account_id.ToString());
      AddSectionEntry(section_user, "Gaia", account.gaia.ToString());
      AddSectionEntry(section_user, "Email", account.email);
      AddSectionEntry(section_user, "Given name", account.given_name);
      AddSectionEntry(section_user, "Hosted domain", account.hosted_domain);
      AddSectionEntry(section_user, "Locale", account.locale);
      AddSectionEntry(
          section_user, "Is subject to parental controls",
          TriboolToString(
              account.capabilities.is_subject_to_parental_controls()));
      AddSectionEntry(section_user, "Is valid", account.IsValid());
      AddSectionEntry(
          section_user, "Is subject to family link parental controls",
          TriboolToString(
              account.capabilities.is_subject_to_parental_controls()));
    }
  }

  base::Value::Dict result;
  result.Set("sections", std::move(section_list));
  FireWebUIListener("basic-info-received", result);

  // Trigger retrieval of the user settings
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  user_settings_subscription_ =
      settings_service->SubscribeForSettingsChange(base::BindRepeating(
          &FamilyLinkUserInternalsMessageHandler::SendFamilyLinkUserSettings,
          weak_factory_.GetWeakPtr()));
}

void FamilyLinkUserInternalsMessageHandler::SendFamilyLinkUserSettings(
    const base::Value::Dict& settings) {
  FireWebUIListener("user-settings-received", settings);
}

void FamilyLinkUserInternalsMessageHandler::SendWebContentFiltersInfo() {
  Profile* profile = Profile::FromWebUI(web_ui());

  AllowJavascript();
  base::Value::Dict info;
  info.Set("enabled", !IsSubjectToFamilyLinkParentalControls(
                          IdentityManagerFactory::GetForProfile(profile)));
  info.Set("search_content_filtering",
           WebContentFiltersToToggle(search_content_filtering_status_));
  info.Set("browser_content_filtering",
           WebContentFiltersToToggle(browser_content_filtering_status_));
  FireWebUIListener("web-content-filters-info-received", info);
}

void FamilyLinkUserInternalsMessageHandler::OnTryURLResult(
    const std::string& callback_id,
    supervised_user::SupervisedUserURLFilter::Result filtering_result) {
  base::Value::Dict result;
  result.Set("allowResult", FilteringResultToString(filtering_result));
  result.Set("manual", filtering_result.IsFromManualList() &&
                           filtering_result.IsAllowed());
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void FamilyLinkUserInternalsMessageHandler::OnURLChecked(
    supervised_user::SupervisedUserURLFilter::Result filtering_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::Dict result;
  result.Set("url", filtering_result.url.possibly_invalid_spec());
  result.Set("result", FilteringResultToString(filtering_result));
  result.Set("reason", FilteringReasonToString(filtering_result));
  FireWebUIListener("filtering-result-received", result);
}
