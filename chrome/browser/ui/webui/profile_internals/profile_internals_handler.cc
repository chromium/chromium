// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

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
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_ui.h"
#include "skia/ext/skia_utils_base.h"

namespace {

base::Value::Dict CreateProfileEntry(
    const ProfileAttributesEntry* entry,
    const base::flat_set<base::FilePath>& loaded_profile_paths,
    const base::flat_set<base::FilePath>& has_off_the_record_profile) {
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
  profile_entry.Set("gaiaId", entry->GetGAIAId());
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
      g_browser_process->profile_manager()->GetKeepAlivesByPath(
          entry->GetPath());
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
  for (const std::string& gaiaId : entry->GetGaiaIds()) {
    signedAccounts.Append(gaiaId);
  }
  profile_entry.Set("signedAccounts", std::move(signedAccounts));
  profile_entry.Set("isLoaded",
                    loaded_profile_paths.contains(entry->GetPath()));
  profile_entry.Set("hasOffTheRecord",
                    has_off_the_record_profile.contains(entry->GetPath()));
  return profile_entry;
}

}  // namespace

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
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  base::flat_set<base::FilePath> loaded_profile_paths =
      base::MakeFlatSet<base::FilePath>(
          loaded_profiles, {}, [](const auto& it) { return it->GetPath(); });
  base::flat_set<base::FilePath> has_off_the_record_profile;
  for (Profile* profile : loaded_profiles) {
    if (profile->GetAllOffTheRecordProfiles().size() > 0) {
      has_off_the_record_profile.insert(profile->GetPath());
    }
  }
  for (const ProfileAttributesEntry* entry : entries) {
    profiles_list.Append(CreateProfileEntry(entry, loaded_profile_paths,
                                            has_off_the_record_profile));
  }
  return profiles_list;
}
