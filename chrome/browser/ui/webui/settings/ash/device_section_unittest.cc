// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/device_section.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_identifier.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace mojom {

using ::chromeos::settings::mojom::Subpage;

}  // namespace mojom

namespace {

constexpr OsSettingsIdentifier kAudioPageOsSettingsId = {
    .subpage = mojom::Subpage::kAudio};

// Provides a correctly formatted result_id based on `SearchConcept`
// configuration in `device_section.cc`. Based on private static function in
// `SearchTagRegistry`.
std::string GetSubpageSearchResultId(OsSettingsIdentifier id, int message_id) {
  std::stringstream ss;
  ss << id.subpage << "," << message_id;
  return ss.str();
}

}  // namespace

// Test for the device settings page.
class DeviceSectionTest : public testing::Test {
 public:
  DeviceSectionTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}
  ~DeviceSectionTest() override = default;

 protected:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("name");
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  TestingProfile* profile() { return profile_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DeviceSection> device_section_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
};

// Verify registry updated with Audio search tags when flag is enabled.
TEST_F(DeviceSectionTest, SearchResultIncludeAudioWithFlagEnabled) {
  feature_list_.InitAndEnableFeature(ash::features::kAudioSettingsPage);
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), pref_service());

  std::string result_id = GetSubpageSearchResultId(
      kAudioPageOsSettingsId, IDS_OS_SETTINGS_TAG_AUDIO_SETTINGS);
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(result_id));
}

// Verify registry not updated with Audio search tags when flag is disabled.
TEST_F(DeviceSectionTest, SearchResultExcludeAudioWithoutFlag) {
  feature_list_.Reset();
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), pref_service());

  std::string result_id = GetSubpageSearchResultId(
      kAudioPageOsSettingsId, IDS_OS_SETTINGS_TAG_AUDIO_SETTINGS);
  EXPECT_FALSE(search_tag_registry()->GetTagMetadata(result_id));
}

}  // namespace ash::settings
