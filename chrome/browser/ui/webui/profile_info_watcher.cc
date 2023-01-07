// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_info_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"

ProfileInfoWatcher::ProfileInfoWatcher(Profile* profile,
                                       base::RepeatingClosure callback)
    : profile_(profile), callback_(std::move(callback)) {
  DCHECK(profile_);
  DCHECK(!callback_.is_null());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The profile_manager might be NULL in testing environments.
  if (profile_manager)
    profile_manager->GetProfileAttributesStorage().AddObserver(this);

  signin_allowed_pref_.Init(
      prefs::kSigninAllowed, profile_->GetPrefs(),
      base::BindRepeating(&ProfileInfoWatcher::RunCallback,
                          base::Unretained(this)));
}

ProfileInfoWatcher::~ProfileInfoWatcher() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The profile_manager might be NULL in testing environments.
  if (profile_manager)
    profile_manager->GetProfileAttributesStorage().RemoveObserver(this);
}

void ProfileInfoWatcher::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  RunCallback();
}

std::string ProfileInfoWatcher::GetAuthenticatedUsername() const {
  std::string username;
  auto* identity_manager = GetIdentityManager();
  if (identity_manager) {
    username =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .email;
  }
  return username;
}

signin::IdentityManager* ProfileInfoWatcher::GetIdentityManager() const {
  return IdentityManagerFactory::GetForProfile(profile_);
}

void ProfileInfoWatcher::RunCallback() {
  if (GetIdentityManager())
    callback_.Run();
}
