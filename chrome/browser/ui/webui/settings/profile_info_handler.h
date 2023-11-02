// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/profiles/profile_statistics_common.h"
#endif

class Profile;

namespace settings {

class ProfileInfoHandler : public SettingsPageUIHandler,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                           public user_manager::UserManager::Observer,
#endif
                           public ProfileAttributesStorage::Observer {
 public:
  static const char kProfileInfoChangedEventName[];
  static const char kProfileStatsCountReadyEventName[];

  explicit ProfileInfoHandler(Profile* profile);

  ProfileInfoHandler(const ProfileInfoHandler&) = delete;
  ProfileInfoHandler& operator=(const ProfileInfoHandler&) = delete;

  ~ProfileInfoHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;
#endif

  // ProfileAttributesStorage::Observer implementation.
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, GetProfileInfo);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, PushProfileInfo);

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::Value::List& args);
  void PushProfileInfo();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void HandleGetProfileStats(const base::Value::List& args);

  // Returns the sum of the counts of individual profile states. Returns 0 if
  // there exists a stat that was not successfully retrieved.
  void PushProfileStatsCount(profiles::ProfileCategoryStats stats);
#endif

  base::Value::Dict GetAccountNameAndIcon();

  // Weak pointer.
  raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
#endif

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<ProfileInfoHandler> callback_weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
