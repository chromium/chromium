// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "skia/ext/skia_utils_base.h"

using regional_capabilities::CountryIdHolder;

// static
std::string ProfileInternalsHandler::CountryIdToDebugString(
    std::optional<CountryIdHolder> country_id) {
  if (!country_id.has_value()) {
    return "not available";
  }
  if (country_id.value() == CountryIdHolder(country_codes::CountryId())) {
    return "unknown";
  }

  return std::string(
      country_id
          ->GetRestricted(regional_capabilities::CountryAccessKey(
              regional_capabilities::CountryAccessReason::
                  kProfileInternalsDisplayInDebugUi))
          .CountryCode());
}

// static
base::Value::Dict ProfileInternalsHandler::CreateProfileEntry(
    const ProfileAttributesEntry* entry) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* loaded_profile = profile_manager->GetProfileByPath(entry->GetPath());

  base::Value::Dict profile_entry;
  profile_entry.Set("profilePath", base::FilePathToValue(entry->GetPath()));
  profile_entry.Set("localProfileName", entry->GetLocalProfileName());
  std::string signin_state;
  switch (entry->GetSigninState()) {
    case SigninState::kNotSignedIn:
      signin_state = "Not signed in";
      break;
    case SigninState::kSignedInWithUnconsentedPrimaryAccount:
      signin_state = "Signed in with unconsented primary account";
      break;
    case SigninState::kSignedInWithConsentedPrimaryAccount:
      signin_state = "Signed in with consented primary account";
      break;
  }
  profile_entry.Set("signinState", signin_state);
  profile_entry.Set("signinRequired", entry->IsSigninRequired());
  // GAIA full name/user name can be empty, if the profile is not signed in to
  // chrome.
  profile_entry.Set("gaiaName", entry->GetGAIAName());
  profile_entry.Set("gaiaId", entry->GetGAIAId().ToString());
  profile_entry.Set("userName", entry->GetUserName());
  profile_entry.Set("hostedDomain", entry->GetHostedDomain());
  profile_entry.Set("isSupervised", entry->IsSupervised());
  profile_entry.Set("isOmitted", entry->IsOmitted());
  profile_entry.Set("isEphemeral", entry->IsEphemeral());
  profile_entry.Set("userAcceptedAccountManagement",
                    entry->UserAcceptedAccountManagement());

  SkColor highlight_color =
      entry->GetProfileThemeColors().profile_highlight_color;
  profile_entry.Set("backgroundColor",
                    skia::SkColorToHexString(highlight_color));
  profile_entry.Set(
      "foregroundColor",
      skia::SkColorToHexString(GetProfileForegroundTextColor(highlight_color)));

  base::Value::List keep_alives;
  std::map<ProfileKeepAliveOrigin, int> keep_alives_map =
      profile_manager->GetKeepAlivesByPath(entry->GetPath());
  for (const auto& pair : keep_alives_map) {
    if (pair.second != 0) {
      std::stringstream ss;
      ss << pair.first;
      base::Value::Dict keep_alive_pair;
      keep_alive_pair.Set("origin", ss.str());
      keep_alive_pair.Set("count", pair.second);
      keep_alives.Append(std::move(keep_alive_pair));
    }
  }
  profile_entry.Set("keepAlives", std::move(keep_alives));

  base::Value::List signedAccounts;
  for (const GaiaId& gaiaId : entry->GetGaiaIds()) {
    signedAccounts.Append(gaiaId.ToString());
  }
  profile_entry.Set("signedAccounts", std::move(signedAccounts));
  profile_entry.Set("isLoaded", loaded_profile != nullptr);
  profile_entry.Set(
      "hasOffTheRecord",
      loaded_profile &&
          loaded_profile->GetAllOffTheRecordProfiles().size() > 0);

  std::optional<CountryIdHolder> profile_country;
  std::optional<CountryIdHolder> initial_keywords_db_country;
  std::optional<CountryIdHolder> updated_keywords_db_country;

  if (loaded_profile) {
    profile_country =
        regional_capabilities::RegionalCapabilitiesServiceFactory::
            GetForProfile(loaded_profile)
                ->GetCountryId();

    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(loaded_profile);
    initial_keywords_db_country =
        template_url_service->initial_keywords_database_country();
    updated_keywords_db_country =
        template_url_service->updated_keywords_database_country();
  }
  profile_entry.Set("profileCountry", CountryIdToDebugString(profile_country));
  profile_entry.Set("initialKeywordsDbCountry",
                    CountryIdToDebugString(initial_keywords_db_country));
  profile_entry.Set("updatedKeywordsDbCountry",
                    CountryIdToDebugString(updated_keywords_db_country));
  profile_entry.Set(
      "variationsCountry",
      g_browser_process->variations_service()
          ? g_browser_process->variations_service()->GetLatestCountry()
          : "not available");
  profile_entry.Set("localeCountry",
                    country_codes::GetCurrentCountryID().CountryCode());

  return profile_entry;
}

ProfileInternalsHandler::ProfileInternalsHandler() = default;

ProfileInternalsHandler::~ProfileInternalsHandler() = default;

void ProfileInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfilesList",
      base::BindRepeating(&ProfileInternalsHandler::HandleGetProfilesList,
                          base::Unretained(this)));
}

void ProfileInternalsHandler::HandleGetProfilesList(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0u, args.size());
  PushProfilesList();
}

void ProfileInternalsHandler::PushProfilesList() {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("profiles-list-changed", GetProfilesList());
}

base::Value::List ProfileInternalsHandler::GetProfilesList() {
  base::Value::List profiles_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByLocalProfileNameWithCheck();
  for (const ProfileAttributesEntry* entry : entries) {
    profiles_list.Append(CreateProfileEntry(entry));
  }
  return profiles_list;
}
