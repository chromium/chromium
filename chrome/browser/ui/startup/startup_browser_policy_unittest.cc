// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}
  ~UnittestProfileManager() override = default;

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    if (!base::PathExists(path) && !base::CreateDirectory(path))
      return nullptr;
    return std::make_unique<TestingProfile>(path);
  }

  std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path,
      Delegate* delegate) override {
    // ThreadTaskRunnerHandle::Get() is TestingProfile's "async" IOTaskRunner
    // (ref. TestingProfile::GetIOTaskRunner()).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));

    return std::make_unique<TestingProfile>(path, this);
  }
};

class StartupBrowserPolicyUnitTest : public testing::Test {
 protected:
  StartupBrowserPolicyUnitTest() = default;
  ~StartupBrowserPolicyUnitTest() override = default;
  base::ScopedTempDir temp_dir_;

  void SetUp() override {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  // Helper function to add a profile with |profile_name| to |profile_manager|'s
  // ProfileAttributesStorage, and return the profile created.
  Profile* CreateTestingProfile(ProfileManager* profile_manager,
                                const std::string& profile_name) {
    ProfileAttributesStorage& storage =
        profile_manager->GetProfileAttributesStorage();
    size_t num_profiles = storage.GetNumberOfProfiles();
    base::FilePath path = temp_dir_.GetPath().AppendASCII(profile_name);
    storage.AddProfile(path, base::ASCIIToUTF16(profile_name.c_str()),
                       std::string(), base::string16(), false, 0, std::string(),
                       EmptyAccountId());
    EXPECT_EQ(num_profiles + 1u, storage.GetNumberOfProfiles());
    return profile_manager->GetProfile(path);
  }

  // Helper function to set profile ephemeral.
  void SetProfileEphemeral(Profile* profile, bool val) {
    ProfileAttributesEntry* entry;
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    EXPECT_TRUE(
        storage.GetProfileAttributesWithPath(profile->GetPath(), &entry));
    entry->SetIsEphemeral(val);
  }

  std::unique_ptr<policy::MockPolicyService> GetPolicyService(
      policy::PolicyMap& policy_map) {
    auto policy_service = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service.get(), GetPolicies(testing::_))
        .WillByDefault(testing::ReturnRef(policy_map));
    return policy_service;
  }

  template <typename... Args>
  void SetPolicy(policy::PolicyMap& policy_map,
                 const std::string& policy,
                 Args... args) {
    policy_map.Set(policy, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(args...), nullptr);
  }

  template <typename... Args>
  std::unique_ptr<policy::PolicyMap> MakePolicy(const std::string& policy,
                                                Args... args) {
    auto policy_map = std::make_unique<policy::PolicyMap>();
    SetPolicy(*policy_map.get(), policy, args...);
    return policy_map;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserPolicyUnitTest);
};

TEST_F(StartupBrowserPolicyUnitTest, BookmarkBarEnabled) {
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, true);
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, false);
  EXPECT_FALSE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, EditBookmarksEnabled) {
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, true);
  EXPECT_TRUE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, false);
  EXPECT_FALSE(welcome::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, DefaultBrowserSettingEnabled) {
  EXPECT_TRUE(welcome::CanShowSetDefaultModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kDefaultBrowserSettingEnabled, true);
  EXPECT_TRUE(welcome::CanShowSetDefaultModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kDefaultBrowserSettingEnabled, false);
  EXPECT_FALSE(welcome::CanShowSetDefaultModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, BrowserSignin) {
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kEnabled));
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBrowserSignin,
                          static_cast<int>(policy::BrowserSigninMode::kForced));
  EXPECT_TRUE(welcome::CanShowSigninModuleForTesting(*policy_map));

  policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kDisabled));
  EXPECT_FALSE(welcome::CanShowSigninModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, ForceEphemeralProfiles) {
  // Needed when building the profile.
  content::BrowserTaskEnvironment task_environment;
  TestingPrefServiceSimple local_state;
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state);
  RegisterLocalState(local_state.registry());

  TestingBrowserProcess::GetGlobal()->SetProfileManager(
      new UnittestProfileManager(temp_dir_.GetPath()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = CreateTestingProfile(profile_manager, "path_1");

  EXPECT_TRUE(welcome::HasModulesToShow(profile));

  SetProfileEphemeral(profile, true);
  EXPECT_FALSE(welcome::HasModulesToShow(profile));

  SetProfileEphemeral(profile, false);
  EXPECT_TRUE(welcome::HasModulesToShow(profile));

  TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(StartupBrowserPolicyUnitTest, NewTabPageLocation) {
  policy::PolicyMap policy_map;
  TestingProfile::Builder builder;
  builder.SetPolicyService(GetPolicyService(policy_map));
  // Needed by the builder when building the profile.
  content::BrowserTaskEnvironment task_environment;
  auto profile = builder.Build();

  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile.get(), base::BindRepeating(&CreateTemplateURLServiceForTesting));

  EXPECT_TRUE(
      welcome::CanShowNTPBackgroundModuleForTesting(policy_map, profile.get()));

  SetPolicy(policy_map, policy::key::kNewTabPageLocation, "https://crbug.com");
  EXPECT_FALSE(
      welcome::CanShowNTPBackgroundModuleForTesting(policy_map, profile.get()));
}
