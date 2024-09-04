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

using content::BrowserThread;

namespace {

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

std::string FilteringBehaviorToString(
    supervised_user::FilteringBehavior behavior,
    bool uncertain) {
  std::string result = FilteringBehaviorToString(behavior);
  if (uncertain)
    result += " (Uncertain)";
  return result;
}

std::string FilteringBehaviorDetailsToString(
    supervised_user::FilteringBehaviorDetails details) {
  switch (details.reason) {
    case supervised_user::FilteringBehaviorReason::DEFAULT:
      return "Default";
    case supervised_user::FilteringBehaviorReason::ASYNC_CHECKER:
      if (details.classification_details.reason ==
          safe_search_api::ClassificationDetails::Reason::kCachedResponse) {
        return "AsyncChecker (Cached)";
      }
      return "AsyncChecker";
    case supervised_user::FilteringBehaviorReason::MANUAL:
      return "Manual";
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
}

void FamilyLinkUserInternalsMessageHandler::OnJavascriptDisallowed() {
  scoped_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void FamilyLinkUserInternalsMessageHandler::OnURLFilterChanged() {
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
  DCHECK(args.empty());
  AllowJavascript();
  if (scoped_observation_.IsObserving())
    return;

  scoped_observation_.Observe(GetSupervisedUserService()->GetURLFilter());
}

void FamilyLinkUserInternalsMessageHandler::HandleGetBasicInfo(
    const base::Value::List& args) {
  SendBasicInfo();
}

void FamilyLinkUserInternalsMessageHandler::HandleTryURL(
    const base::Value::List& args) {
  DCHECK_EQ(2u, args.size());
  if (!args[0].is_string() || !args[1].is_string())
    return;
  const std::string& callback_id = args[0].GetString();
  const std::string& url_str = args[1].GetString();

  GURL url = url_formatter::FixupURL(url_str, std::string());
  if (!url.is_valid())
    return;

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

  filter->GetFilteringBehaviorForURLWithAsyncChecks(
      url,
      base::BindOnce(&FamilyLinkUserInternalsMessageHandler::OnTryURLResult,
                     weak_factory_.GetWeakPtr(), callback_id),
      skip_manual_parent_filter);
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
  AddSectionEntry(
      section_filter, "Default behavior",
      FilteringBehaviorToString(filter->GetDefaultFilteringBehavior()));

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
      AddSectionEntry(section_user, "Gaia", account.gaia);
      AddSectionEntry(section_user, "Email", account.email);
      AddSectionEntry(section_user, "Given name", account.given_name);
      AddSectionEntry(section_user, "Hosted domain", account.hosted_domain);
      AddSectionEntry(section_user, "Locale", account.locale);
      AddSectionEntry(
          section_user, "Is subject to parental controls",
          TriboolToString(
              account.capabilities.is_subject_to_parental_controls()));
      AddSectionEntry(section_user, "Is valid", account.IsValid());
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

void FamilyLinkUserInternalsMessageHandler::OnTryURLResult(
    const std::string& callback_id,
    supervised_user::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorReason reason,
    bool uncertain) {
  base::Value::Dict result;
  result.Set("allowResult", FilteringBehaviorToString(behavior, uncertain));
  result.Set("manual",
             reason == supervised_user::FilteringBehaviorReason::MANUAL &&
                 behavior == supervised_user::FilteringBehavior::kAllow);
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void FamilyLinkUserInternalsMessageHandler::OnURLChecked(
    const GURL& url,
    supervised_user::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorDetails details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value::Dict result;
  result.Set("url", url.possibly_invalid_spec());
  result.Set("result",
             FilteringBehaviorToString(
                 behavior, details.classification_details.reason ==
                               safe_search_api::ClassificationDetails::Reason::
                                   kFailedUseDefault));
  result.Set("reason", FilteringBehaviorDetailsToString(details));
  FireWebUIListener("filtering-result-received", result);
}
