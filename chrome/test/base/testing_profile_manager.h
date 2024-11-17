// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/test/scoped_path_override.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class ProfileAttributesStorage;
class ProfileManager;
class TestingBrowserProcess;

namespace sync_preferences {
class PrefServiceSyncable;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class AccountProfileMapper;
#endif

// The TestingProfileManager is a TestingProfile factory for a multi-profile
// environment. It will bring up a full ProfileManager and attach it to the
// TestingBrowserProcess set up in your test.
//
// When a Profile is needed for testing, create it through the factory method
// below instead of creating it via |new TestingProfile|. It is not possible
// to register profiles created in that fashion with the ProfileManager.
class TestingProfileManager : public ProfileObserver {
 public:
  explicit TestingProfileManager(TestingBrowserProcess* browser_process);
  TestingProfileManager(TestingBrowserProcess* browser_process,
                        ScopedTestingLocalState* local_state);
  TestingProfileManager(const TestingProfileManager&) = delete;
  TestingProfileManager& operator=(const TestingProfileManager&) = delete;
  ~TestingProfileManager() override;

  // This needs to be called in testing::Test::SetUp() to put the object in a
  // valid state. Some work cannot be done in a constructor because it may
  // call gtest asserts to verify setup. The result of this call can be used
  // to ASSERT before doing more SetUp work in the test.
  // |profiles_dir| is the path in which new directories would be placed.
  // If empty, one will be created (and deleted upon destruction of |this|).
  // If not empty, it will be used, but ownership is maintained by the caller.
  // If `profile_manager` is supplied, then it will be set as |profile_manager|
  // of this TestingProfileManager, instead of creating a new one in
  // SetUpInternal().
  [[nodiscard]] bool SetUp(
      const base::FilePath& profiles_path = base::FilePath(),
      std::unique_ptr<ProfileManager> profile_manager = nullptr);

  // Creates a new TestingProfile whose data lives in a directory related to
  // profile_name, which is a non-user-visible key for the test environment.
  // |prefs| is the PrefService used by the profile. If it is NULL, the profile
  // creates a PrefService on demand.
  // |user_name|, |avatar_id| and |is_supervised_profile| status are passed
  // along to the ProfileAttributesStorage and provide the user-visible profile
  // metadata. This will register the TestingProfile with the profile subsystem
  // as well. The subsystem owns the Profile and returns a weak pointer.
  // |factories| contains BCKSs to use with the newly created profile.
  TestingProfile* CreateTestingProfile(
      const std::string& profile_name,
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
      const std::u16string& user_name,
      int avatar_id,
      TestingProfile::TestingFactories testing_factories,
      bool is_supervised_profile = false,
      std::optional<bool> is_new_profile = std::nullopt,
      std::optional<std::unique_ptr<policy::PolicyService>> policy_service =
          std::nullopt,
      bool is_main_profile = false,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
          nullptr);

  // Small helpers for creating testing profiles. Just forward to above.
  TestingProfile* CreateTestingProfile(
      const std::string& name,
      bool is_main_profile = false,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
          nullptr);
  TestingProfile* CreateTestingProfile(
      const std::string& name,
      TestingProfile::TestingFactories testing_factories,
      bool is_main_profile = false,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
          nullptr);

  // Creates a new guest TestingProfile whose data lives in the guest profile
  // test environment directory, as specified by the profile manager.
  // This profile will not be added to the ProfileAttributesStorage. This will
  // register the TestingProfile with the profile subsystem as well.
  // The subsystem owns the Profile and returns a weak pointer.
  TestingProfile* CreateGuestProfile();

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  // Creates a new system TestingProfile whose data lives in the system profile
  // test environment directory, as specified by the profile manager.
  // This profile will not be added to the ProfileAttributesStorage. This will
  // register the TestingProfile with the profile subsystem as well.
  // The subsystem owns the Profile and returns a weak pointer.
  TestingProfile* CreateSystemProfile();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

  // Deletes a TestingProfile from the profile subsystem.
  void DeleteTestingProfile(const std::string& profile_name);

  // Deletes all TestingProfiles from the profile subsystem, including guest
  // profiles.
  void DeleteAllTestingProfiles();

  // Deletes a guest TestingProfile from the profile manager.
  void DeleteGuestProfile();

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  // Deletes a system TestingProfile from the profile manager.
  void DeleteSystemProfile();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

  // Deletes the storage instance. This is useful for testing that the storage
  // is properly persisting data.
  void DeleteProfileAttributesStorage();

  // Get the full profile path from the profile name.
  base::FilePath GetProfilePath(const std::string& profile_name);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetAccountProfileMapper(std::unique_ptr<AccountProfileMapper> mapper);
#endif

  // Helper accessors.
  const base::FilePath& profiles_dir();
  ProfileManager* profile_manager();
  ProfileAttributesStorage* profile_attributes_storage();
  ScopedTestingLocalState* local_state() { return local_state_; }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  friend class ProfileAttributesStorageTest;
  friend class ProfileNameVerifierObserver;

  typedef std::map<std::string, raw_ptr<TestingProfile, CtnExperimental>>
      TestingProfilesMap;

  // Does the actual ASSERT-checked SetUp work. This function cannot have a
  // return value, so it sets the |called_set_up_| flag on success and that is
  // returned in the public SetUp.
  void SetUpInternal(const base::FilePath& profiles_path,
                     std::unique_ptr<ProfileManager> profile_manager);

  // Whether SetUp() was called to put the object in a valid state.
  bool called_set_up_;

  // |profiles_path_| is the path under which new directories for the profiles
  // will be placed.
  base::FilePath profiles_path_;

  // The user data directory in the path service is overriden because some
  // functions, e.g. GetPathOfHighResAvatarAtIndex, get the user data directory
  // by the path service instead of the profile manager. The override is scoped
  // with the help of this variable.
  std::unique_ptr<base::ScopedPathOverride> user_data_dir_override_;

  // Weak reference to the browser process on which the ProfileManager is set.
  raw_ptr<TestingBrowserProcess> browser_process_;

  // Local state in which all the profiles are registered.
  raw_ptr<ScopedTestingLocalState> local_state_;

  // Owned local state for when it's not provided in the constructor.
  std::unique_ptr<ScopedTestingLocalState> owned_local_state_;

  // Weak reference to the profile manager.
  raw_ptr<ProfileManager, DanglingUntriaged> profile_manager_;

  // Map of profile_name to TestingProfile* from CreateTestingProfile().
  TestingProfilesMap testing_profiles_;

  // Listens for Profile* destruction to perform some cleanup.
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
