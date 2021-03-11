// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

const char kGuestProfileName[] = "Guest";
const char kSystemProfileName[] = "System";

namespace testing {

class ProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit ProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    return std::make_unique<TestingProfile>(path);
  }
};

}  // namespace testing

TestingProfileManager::TestingProfileManager(TestingBrowserProcess* process)
    : called_set_up_(false),
      browser_process_(process),
      owned_local_state_(std::make_unique<ScopedTestingLocalState>(process)),
      profile_manager_(nullptr) {
  local_state_ = owned_local_state_.get();
}

TestingProfileManager::TestingProfileManager(
    TestingBrowserProcess* process,
    ScopedTestingLocalState* local_state)
    : called_set_up_(false),
      browser_process_(process),
      local_state_(local_state),
      profile_manager_(nullptr) {}

TestingProfileManager::~TestingProfileManager() {
  // Destroying this class also destroys the LocalState, so make sure the
  // associated ProfileManager is also destroyed.
  browser_process_->SetProfileManager(NULL);
}

bool TestingProfileManager::SetUp(const base::FilePath& profiles_path) {
  SetUpInternal(profiles_path);
  return called_set_up_;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& profile_name,
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    const std::u16string& user_name,
    int avatar_id,
    const std::string& supervised_user_id,
    TestingProfile::TestingFactories testing_factories,
    base::Optional<bool> is_new_profile,
    base::Optional<std::unique_ptr<policy::PolicyService>> policy_service) {
  DCHECK(called_set_up_);

  // Create a path for the profile based on the name.
  base::FilePath profile_path(profiles_path_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_name != chrome::kInitialProfile &&
      profile_name != chrome::kLockScreenProfile &&
      profile_name != chromeos::ProfileHelper::GetLockScreenAppProfileName()) {
    profile_path =
        profile_path.Append(chromeos::ProfileHelper::Get()->GetUserProfileDir(
            chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
                profile_name)));
  } else {
    profile_path = profile_path.AppendASCII(profile_name);
  }
#else
  profile_path = profile_path.AppendASCII(profile_name);
#endif

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(profile_path);
  builder.SetPrefService(std::move(prefs));
  builder.SetSupervisedUserId(supervised_user_id);
  builder.SetProfileName(profile_name);
  builder.SetIsNewProfile(is_new_profile.value_or(false));
  if (policy_service)
    builder.SetPolicyService(std::move(*policy_service));

  for (TestingProfile::TestingFactories::value_type& pair : testing_factories)
    builder.AddTestingFactory(pair.first, std::move(pair.second));
  testing_factories.clear();

  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_manager_->AddProfile(std::move(profile));

  // Update the user metadata.
  ProfileAttributesEntry* entry =
      profile_manager_->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);
  entry->SetAvatarIconIndex(avatar_id);
  entry->SetSupervisedUserId(supervised_user_id);
  entry->SetLocalProfileName(user_name, entry->IsUsingDefaultName());

  testing_profiles_.insert(std::make_pair(profile_name, profile_ptr));

  return profile_ptr;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(name, /*testing_factories=*/{});
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name,
    TestingProfile::TestingFactories testing_factories) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(
      name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16(name), 0, std::string(), std::move(testing_factories));
}

TestingProfile* TestingProfileManager::CreateGuestProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());

  // Add the guest profile to the profile manager, but not to the info cache.
  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_ptr->set_profile_name(kGuestProfileName);

  // Set up a profile with an off the record profile.
  if (!TestingProfile::IsEphemeralGuestProfileEnabled()) {
    TestingProfile::Builder off_the_record_builder;
    off_the_record_builder.SetGuestSession();
    off_the_record_builder.BuildIncognito(profile_ptr);
  }

  profile_manager_->AddProfile(std::move(profile));
  profile_manager_->SetNonPersonalProfilePrefs(profile_ptr);

  testing_profiles_.insert(std::make_pair(kGuestProfileName, profile_ptr));

  return profile_ptr;
}

TestingProfile* TestingProfileManager::CreateSystemProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(ProfileManager::GetSystemProfilePath());

  // Add the system profile to the profile manager, but not to the info cache.
  std::unique_ptr<TestingProfile> profile = builder.Build();
  TestingProfile* profile_ptr = profile.get();
  profile_ptr->set_profile_name(kSystemProfileName);

  profile_manager_->AddProfile(std::move(profile));

  testing_profiles_.insert(std::make_pair(kSystemProfileName, profile_ptr));

  return profile_ptr;
}

void TestingProfileManager::DeleteTestingProfile(const std::string& name) {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(name);
  DCHECK(it != testing_profiles_.end());

  TestingProfile* profile = it->second;

  profile_manager_->GetProfileAttributesStorage().RemoveProfile(
      profile->GetPath());

  profile_manager_->profiles_info_.erase(profile->GetPath());

  testing_profiles_.erase(it);
}

void TestingProfileManager::DeleteAllTestingProfiles() {
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  for (auto it = testing_profiles_.begin(); it != testing_profiles_.end();
       ++it) {
    TestingProfile* profile = it->second;
    storage.RemoveProfile(profile->GetPath());
  }
  testing_profiles_.clear();
}


void TestingProfileManager::DeleteGuestProfile() {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(kGuestProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(ProfileManager::GetGuestProfilePath());
}

void TestingProfileManager::DeleteSystemProfile() {
  DCHECK(called_set_up_);

  auto it = testing_profiles_.find(kSystemProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(
      ProfileManager::GetSystemProfilePath());
}

void TestingProfileManager::DeleteProfileInfoCache() {
  profile_manager_->profile_info_cache_.reset(NULL);
}

void TestingProfileManager::UpdateLastUser(Profile* last_active) {
#if !defined(OS_ANDROID)
  profile_manager_->UpdateLastUser(last_active);
#endif
}

const base::FilePath& TestingProfileManager::profiles_dir() {
  DCHECK(called_set_up_);
  return profiles_path_;
}

ProfileManager* TestingProfileManager::profile_manager() {
  DCHECK(called_set_up_);
  return profile_manager_;
}

ProfileInfoCache* TestingProfileManager::profile_info_cache() {
  DCHECK(called_set_up_);
  return &profile_manager_->GetProfileInfoCache();
}

ProfileAttributesStorage* TestingProfileManager::profile_attributes_storage() {
  return profile_info_cache();
}

void TestingProfileManager::SetUpInternal(const base::FilePath& profiles_path) {
  ASSERT_FALSE(browser_process_->profile_manager())
      << "ProfileManager already exists";

  // Set up the directory for profiles.
  if (profiles_path.empty()) {
    profiles_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  } else {
    profiles_path_ = profiles_path;
  }
  user_data_dir_override_ = std::make_unique<base::ScopedPathOverride>(
      chrome::DIR_USER_DATA, profiles_path_);

  profile_manager_ = new testing::ProfileManager(profiles_path_);
  browser_process_->SetProfileManager(profile_manager_);  // Takes ownership.

  profile_manager_->GetProfileInfoCache().
      set_disable_avatar_download_for_testing(true);
  called_set_up_ = true;
}
