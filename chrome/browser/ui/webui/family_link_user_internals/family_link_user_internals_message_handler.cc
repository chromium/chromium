// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_message_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
base::ListValue* AddSection(base::ListValue* parent_list,
                            const std::string& title) {
  std::unique_ptr<base::DictionaryValue> section(new base::DictionaryValue);
  std::unique_ptr<base::ListValue> section_contents(new base::ListValue);
  section->SetString("title", title);
  // Grab a raw pointer to the result before |Pass()|ing it on.
  base::ListValue* result =
      section->SetList("data", std::move(section_contents));
  parent_list->Append(std::move(section));
  return result;
}

// Adds a bool entry to a section (created with |AddSection| above).
void AddSectionEntry(base::ListValue* section_list,
                     const std::string& name,
                     bool value) {
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->SetString("stat_name", name);
  entry->SetBoolean("stat_value", value);
  entry->SetBoolean("is_valid", true);
  section_list->Append(std::move(entry));
}

// Adds a string entry to a section (created with |AddSection| above).
void AddSectionEntry(base::ListValue* section_list,
                     const std::string& name,
                     const std::string& value) {
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->SetString("stat_name", name);
  entry->SetString("stat_value", value);
  entry->SetBoolean("is_valid", true);
  section_list->Append(std::move(entry));
}

std::string FilteringBehaviorToString(
    SupervisedUserURLFilter::FilteringBehavior behavior) {
  switch (behavior) {
    case SupervisedUserURLFilter::ALLOW:
      return "Allow";
    case SupervisedUserURLFilter::WARN:
      return "Warn";
    case SupervisedUserURLFilter::BLOCK:
      return "Block";
    case SupervisedUserURLFilter::INVALID:
      return "Invalid";
  }
  return "Unknown";
}

std::string FilteringBehaviorToString(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    bool uncertain) {
  std::string result = FilteringBehaviorToString(behavior);
  if (uncertain)
    result += " (Uncertain)";
  return result;
}

std::string FilteringBehaviorReasonToString(
    supervised_user_error_page::FilteringBehaviorReason reason) {
  switch (reason) {
    case supervised_user_error_page::DEFAULT:
      return "Default";
    case supervised_user_error_page::ASYNC_CHECKER:
      return "AsyncChecker";
    case supervised_user_error_page::DENYLIST:
      return "Denylist";
    case supervised_user_error_page::MANUAL:
      return "Manual";
    case supervised_user_error_page::ALLOWLIST:
      return "Allowlist";
    case supervised_user_error_page::NOT_SIGNED_IN:
      // Should never happen, only used for requests from WebView
      NOTREACHED();
  }
  return "Unknown/invalid";
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

SupervisedUserService*
FamilyLinkUserInternalsMessageHandler::GetSupervisedUserService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return SupervisedUserServiceFactory::GetForProfile(
      profile->GetOriginalProfile());
}

void FamilyLinkUserInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();
  if (scoped_observation_.IsObserving())
    return;

  scoped_observation_.Observe(GetSupervisedUserService()->GetURLFilter());
}

void FamilyLinkUserInternalsMessageHandler::HandleGetBasicInfo(
    const base::ListValue* args) {
  SendBasicInfo();
}

void FamilyLinkUserInternalsMessageHandler::HandleTryURL(
    const base::ListValue* args) {
  DCHECK_EQ(2u, args->GetSize());
  std::string callback_id;
  std::string url_str;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &url_str))
    return;

  GURL url = url_formatter::FixupURL(url_str, std::string());
  if (!url.is_valid())
    return;

  SupervisedUserURLFilter* filter = GetSupervisedUserService()->GetURLFilter();
  content::WebContents* web_contents =
      web_ui() ? web_ui()->GetWebContents() : nullptr;
  bool skip_manual_parent_filter = false;
  if (web_contents) {
    skip_manual_parent_filter =
        filter->ShouldSkipParentManualAllowlistFiltering(
            web_contents->GetOutermostWebContents());
  }

  std::map<std::string, std::u16string> allowlists =
      filter->GetMatchingAllowlistTitles(url);
  filter->GetFilteringBehaviorForURLWithAsyncChecks(
      url,
      base::BindOnce(&FamilyLinkUserInternalsMessageHandler::OnTryURLResult,
                     weak_factory_.GetWeakPtr(), allowlists, callback_id),
      skip_manual_parent_filter);
}

void FamilyLinkUserInternalsMessageHandler::SendBasicInfo() {
  std::unique_ptr<base::ListValue> section_list(new base::ListValue);

  base::ListValue* section_general = AddSection(section_list.get(), "General");
  AddSectionEntry(section_general, "Child detection enabled",
                  ChildAccountService::IsChildAccountDetectionEnabled());

  Profile* profile = Profile::FromWebUI(web_ui());

  base::ListValue* section_profile = AddSection(section_list.get(), "Profile");
  AddSectionEntry(section_profile, "Account", profile->GetProfileUserName());
  AddSectionEntry(section_profile, "Child", profile->IsChild());

  SupervisedUserURLFilter* filter = GetSupervisedUserService()->GetURLFilter();

  base::ListValue* section_filter = AddSection(section_list.get(), "Filter");
  AddSectionEntry(section_filter, "Denylist active", filter->HasDenylist());
  AddSectionEntry(section_filter, "Online checks active",
                  filter->HasAsyncURLChecker());
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
      base::ListValue* section_user = AddSection(
          section_list.get(), "User Information for " + account.full_name);
      AddSectionEntry(section_user, "Account id",
                      account.account_id.ToString());
      AddSectionEntry(section_user, "Gaia", account.gaia);
      AddSectionEntry(section_user, "Email", account.email);
      AddSectionEntry(section_user, "Given name", account.given_name);
      AddSectionEntry(section_user, "Hosted domain", account.hosted_domain);
      AddSectionEntry(section_user, "Locale", account.locale);
      AddSectionEntry(section_user, "Is child", account.is_child_account);
      AddSectionEntry(section_user, "Is valid", account.IsValid());
    }
  }

  base::DictionaryValue result;
  result.Set("sections", std::move(section_list));
  FireWebUIListener("basic-info-received", result);

  // Trigger retrieval of the user settings
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  user_settings_subscription_ =
      settings_service->SubscribeForSettingsChange(base::BindRepeating(
          &FamilyLinkUserInternalsMessageHandler::SendFamilyLinkUserSettings,
          weak_factory_.GetWeakPtr()));
}

void FamilyLinkUserInternalsMessageHandler::SendFamilyLinkUserSettings(
    const base::DictionaryValue* settings) {
  FireWebUIListener(
      "user-settings-received",
      *(settings ? settings : std::make_unique<base::Value>().get()));
}

void FamilyLinkUserInternalsMessageHandler::OnTryURLResult(
    const std::map<std::string, std::u16string>& allowlists,
    const std::string& callback_id,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  std::vector<std::string> allowlists_list;
  for (const auto& allowlist : allowlists) {
    allowlists_list.push_back(
        base::StringPrintf("%s: %s", allowlist.first.c_str(),
                           base::UTF16ToUTF8(allowlist.second).c_str()));
  }
  std::string allowlists_str = base::JoinString(allowlists_list, "; ");
  base::DictionaryValue result;
  result.SetString("allowResult",
                   FilteringBehaviorToString(behavior, uncertain));
  result.SetBoolean("manual", reason == supervised_user_error_page::MANUAL &&
                                  behavior == SupervisedUserURLFilter::ALLOW);
  result.SetString("allowlists", allowlists_str);
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void FamilyLinkUserInternalsMessageHandler::OnSiteListUpdated() {}

void FamilyLinkUserInternalsMessageHandler::OnURLChecked(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::DictionaryValue result;
  result.SetString("url", url.possibly_invalid_spec());
  result.SetString("result", FilteringBehaviorToString(behavior, uncertain));
  result.SetString("reason", FilteringBehaviorReasonToString(reason));
  FireWebUIListener("filtering-result-received", result);
}
