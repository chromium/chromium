// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/crostini/crostini_section.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

// Test for Crostini settings. Currently this only tests the Bruschetta section.
class CrostiniSectionTest : public testing::Test {
 public:
  CrostiniSectionTest() = default;
  ~CrostiniSectionTest() override = default;

 protected:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test profile");
  }

  void TearDown() override {
    profile_manager_->DeleteTestingProfile("test profile");
  }

  void EnableBruschetta() {
    feature_list_.InitAndEnableFeature(ash::features::kBruschetta);
  }

  void DisableBruschetta() {
    feature_list_.InitAndDisableFeature(ash::features::kBruschetta);
  }

  void AddInstallableConfig() {
    base::Value::Dict pref;
    base::Value::Dict config;
    config.Set(bruschetta::prefs::kPolicyEnabledKey,
               static_cast<int>(
                   bruschetta::prefs::PolicyEnabledState::INSTALL_ALLOWED));
    config.Set(bruschetta::prefs::kPolicyNameKey, "Config name");
    pref.Set("test-config", std::move(config));
    profile_->GetPrefs()->SetDict(bruschetta::prefs::kBruschettaVMConfiguration,
                                  std::move(pref));
  }

  void AddInstall() {
    const guest_os::GuestId guest_id(guest_os::VmType::BRUSCHETTA, "vm_name",
                                     "container_name");
    AddContainerToPrefs(profile_, guest_id, /*properties=*/{});
  }

  bool ShouldShowBruschetta() {
    return CrostiniSection::ShouldShowBruschetta(profile_);
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
};

// Test that Bruschetta doesn't show up when it's disabled by flags.
TEST_F(CrostiniSectionTest, BruschettaDisabled) {
  DisableBruschetta();
  EXPECT_FALSE(ShouldShowBruschetta());
  AddInstallableConfig();
  EXPECT_FALSE(ShouldShowBruschetta());
  AddInstall();
  EXPECT_FALSE(ShouldShowBruschetta());
}

// Test that Bruschetta shows up when it has an installable config.
TEST_F(CrostiniSectionTest, BruschettaEnabled) {
  EnableBruschetta();
  EXPECT_FALSE(ShouldShowBruschetta());
  AddInstallableConfig();
  EXPECT_TRUE(ShouldShowBruschetta());
}

// Test that Bruschetta does show up when it's installed, despite not having an
// installable config.
TEST_F(CrostiniSectionTest, BruschettaInstalled) {
  EnableBruschetta();
  AddInstall();
  EXPECT_TRUE(ShouldShowBruschetta());
}

}  // namespace ash::settings
