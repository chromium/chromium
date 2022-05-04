// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

#include "base/bind.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_ui.h"

namespace {

base::Value CreateProfileEntry(const ProfileAttributesEntry* entry) {
  base::Value profile_entry(base::Value::Type::DICTIONARY);
  profile_entry.SetKey("profilePath", base::FilePathToValue(entry->GetPath()));
  profile_entry.SetStringKey("localProfileName", entry->GetLocalProfileName());
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
  profile_entry.SetStringKey("signinState", signin_state);
  profile_entry.SetBoolKey("signinRequired", entry->IsSigninRequired());
  // GAIA full name/user name can be empty, if the profile is not signed in to
  // chrome.
  profile_entry.SetStringKey("gaiaName", entry->GetGAIAName());
  profile_entry.SetStringKey("gaiaId", entry->GetGAIAId());
  profile_entry.SetStringKey("userName", entry->GetUserName());
  profile_entry.SetStringKey("hostedDomain", entry->GetHostedDomain());
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

base::Value ProfileInternalsHandler::GetProfilesList() {
  base::Value profiles_list(base::Value::Type::LIST);
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByLocalProfileName();
  for (const ProfileAttributesEntry* entry : entries) {
    profiles_list.Append(CreateProfileEntry(entry));
  }
  return profiles_list;
}
