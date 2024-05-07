// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

namespace {

class ProfileNameChangeFuture : public base::test::TestFuture<std::u16string>,
                                public ProfileAttributesStorage::Observer {
 public:
  // Creates a future that will be resolved with the profile name the next time
  // it's updated. If `observed_profile_path` is empty, any profile name update
  // will result in the future resolving.
  explicit ProfileNameChangeFuture(
      ProfileAttributesStorage& profile_attributes_storage,
      const base::FilePath& observed_profile_path = base::FilePath())
      : profile_attributes_storage_(profile_attributes_storage),
        observed_profile_path_(observed_profile_path) {
    scoped_observation_.Observe(&profile_attributes_storage_.get());
  }

  // ProfileAttributesStorage::Observer:
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override {
    if (!observed_profile_path_.empty() &&
        observed_profile_path_ != profile_path) {
      return;
    }

    scoped_observation_.Reset();
    std::move(GetCallback())
        .Run(profile_attributes_storage_
                 ->GetProfileAttributesWithPath(profile_path)
                 ->GetLocalProfileName());
  }

 private:
  const raw_ref<ProfileAttributesStorage> profile_attributes_storage_;
  const base::FilePath observed_profile_path_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      scoped_observation_{this};
};

}  // namespace

class FirstRunServiceTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FirstRunServiceTest, ShouldOpenFirstRun) {
  TestingProfileManager profile_manager{TestingBrowserProcess::GetGlobal()};
  ASSERT_TRUE(profile_manager.SetUp());

  auto* profile = profile_manager.CreateTestingProfile("Test Profile");
  EXPECT_TRUE(ShouldOpenFirstRun(profile));

  SetIsFirstRun(false);
  EXPECT_FALSE(ShouldOpenFirstRun(profile));

  SetIsFirstRun(true);
  EXPECT_TRUE(ShouldOpenFirstRun(profile));

  g_browser_process->local_state()->SetBoolean(prefs::kFirstRunFinished, true);
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
}

// Regression test for crbug.com/1450709.
TEST_F(FirstRunServiceTest, ShouldPopulateProfileNameFromPrimaryAccount) {
  signin::IdentityTestEnvironment identity_test_env;
  TestingProfileManager testing_profile_manager{
      TestingBrowserProcess::GetGlobal()};
  ASSERT_TRUE(testing_profile_manager.SetUp());

  Profile* profile =
      testing_profile_manager.CreateTestingProfile("Test Profile");

  AccountInfo primary_account_info = identity_test_env.MakeAccountAvailable(
      "primary@gmail.com",
      {.primary_account_consent_level = signin::ConsentLevel::kSync});
  AccountInfo secondary_account_info =
      identity_test_env.MakeAccountAvailable("secondary@gmail.com");

  ProfileNameChangeFuture profile_name_future(
      testing_profile_manager.profile_manager()->GetProfileAttributesStorage(),
      profile->GetPath());

  // Note: the identity manager is not connected to the profile, but for this
  // test, it's not necessary.
  auto first_run_service =
      FirstRunService(*profile, *identity_test_env.identity_manager());

  // Run and complete the first run.
  base::RunLoop fre_completion_loop;
  first_run_service.TryMarkFirstRunAlreadyFinished(
      fre_completion_loop.QuitClosure());
  fre_completion_loop.Run();
  EXPECT_FALSE(ShouldOpenFirstRun(profile));

  // The profile name is still unchanged.
  EXPECT_FALSE(profile_name_future.IsReady());

  // Send extended account info, starting with the secondary account.
  identity_test_env.UpdateAccountInfoForAccount(
      signin::WithGeneratedUserInfo(secondary_account_info, "Secondary"));
  identity_test_env.UpdateAccountInfoForAccount(
      signin::WithGeneratedUserInfo(primary_account_info, "Primary"));

  // The profile name should now be resolved.
  EXPECT_TRUE(profile_name_future.IsReady());
  EXPECT_EQ(u"Primary", profile_name_future.Get());
}
