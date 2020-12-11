// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILE_INFO_WATCHER_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILE_INFO_WATCHER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/prefs/pref_member.h"

class Profile;

namespace signin {
class IdentityManager;
}

// Watches profiles for changes in their cached info (e.g. the authenticated
// username changes).
class ProfileInfoWatcher : public ProfileAttributesStorage::Observer {
 public:
  ProfileInfoWatcher(Profile* profile, const base::Closure& callback);
  ~ProfileInfoWatcher() override;

  // Gets the authenticated username (e.g. username@gmail.com) for |profile_|.
  std::string GetAuthenticatedUsername() const;

 private:
  // ProfileAttributesStorage::Observer:
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;

  // Gets the IdentityManager for |profile_|.
  signin::IdentityManager* GetIdentityManager() const;

  // Runs |callback_| when a profile changes. No-ops if |GetIdentityManager()|
  // returns nullptr.
  void RunCallback();

  // Weak reference to the profile this class observes.
  Profile* const profile_;

  // Called when the authenticated username changes.
  base::Closure callback_;

  BooleanPrefMember signin_allowed_pref_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoWatcher);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILE_INFO_WATCHER_H_
