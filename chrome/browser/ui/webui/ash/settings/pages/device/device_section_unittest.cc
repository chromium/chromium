// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/device_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_identifier.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"

namespace ash::settings {

namespace mojom {

using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;

}  // namespace mojom

namespace {

constexpr OsSettingsIdentifier kAudioPageOsSettingsId = {
    .subpage = mojom::Subpage::kAudio};
constexpr OsSettingsIdentifier kKeyboardOsSettingsId = {
    .subpage = mojom::Subpage::kKeyboard};
constexpr OsSettingsIdentifier kPerDeviceKeyboardOsSettingsId = {
    .subpage = mojom::Subpage::kPerDeviceKeyboard};
constexpr OsSettingsIdentifier kKeyboardBlockMetaFkeyRewritesOsSettingsId = {
    .setting = mojom::Setting::kKeyboardBlockMetaFkeyRewrites};
constexpr OsSettingsIdentifier kAddPrinterId = {
    .setting = mojom::Setting::kAddPrinter};
constexpr OsSettingsIdentifier kPrintingDetailsId = {
    .subpage = mojom::Subpage::kPrintingDetails};
constexpr OsSettingsIdentifier kPrintJobsId = {.setting =
                                                   mojom::Setting::kPrintJobs};
constexpr OsSettingsIdentifier kScanningAppId = {
    .setting = mojom::Setting::kScanningApp};

// Provides a correctly formatted result_id based on `SearchConcept`
// configuration in `device_section.cc`. Based on private static function in
// `SearchTagRegistry`.
std::string GetSubpageSearchResultId(OsSettingsIdentifier id, int message_id) {
  std::stringstream ss;
  ss << id.subpage << "," << message_id;
  return ss.str();
}

std::string GetSettingsSearchResultId(OsSettingsIdentifier id, int message_id) {
  std::stringstream ss;
  ss << id.setting << "," << message_id;
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

    // Mock Input method manager and register prefs for Inputs settings
    input_method::InitializeForTesting(
        new input_method::MockInputMethodManagerImpl());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kEmojiSuggestionEnterpriseAllowed, true);
    pref_service_.registry()->RegisterBooleanPref(
        spellcheck::prefs::kSpellCheckEnable, true);
  }

  void TearDown() override {
    // Ensure `device_section_` is destroyed before `pref_service_`.
    device_section_.reset();

    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  TestingProfile* profile() { return profile_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }
  FakeCupsPrintersManager* printers_manager() { return &printers_manager_; }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DeviceSection> device_section_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  FakeCupsPrintersManager printers_manager_;
};

// Verify registry updated with Audio search tags.
TEST_F(DeviceSectionTest, SearchResultIncludeAudio) {
  feature_list_.Reset();
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), printers_manager(), pref_service());

  std::string result_id = GetSubpageSearchResultId(
      kAudioPageOsSettingsId, IDS_OS_SETTINGS_TAG_AUDIO_SETTINGS);
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(result_id));
}

// Verify registry updated with Printing search tags.
TEST_F(DeviceSectionTest, SearchResultIncludePrinting) {
  feature_list_.InitAndEnableFeature(
      ash::features::kOsSettingsRevampWayfinding);
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), printers_manager(), pref_service());

  std::string add_printer_result_id = GetSettingsSearchResultId(
      kAddPrinterId, IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER);
  std::string printing_details_result_id = GetSubpageSearchResultId(
      kPrintingDetailsId, IDS_OS_SETTINGS_TAG_PRINTING);
  std::string print_jobs_result_id = GetSettingsSearchResultId(
      kPrintJobsId, IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT);
  std::string scanning_app_result_id = GetSettingsSearchResultId(
      kScanningAppId, IDS_OS_SETTINGS_TAG_SCANNING_APP);

  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(add_printer_result_id));
  EXPECT_TRUE(
      search_tag_registry()->GetTagMetadata(printing_details_result_id));
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(print_jobs_result_id));
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(scanning_app_result_id));
}

// Verify registry updated with per device settings search tags when flag is
// enabled.
TEST_F(DeviceSectionTest, SearchResultChangeToSettingsSplitWithFlag) {
  feature_list_.InitAndEnableFeature(ash::features::kInputDeviceSettingsSplit);
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), printers_manager(), pref_service());

  std::string result_id = GetSubpageSearchResultId(
      kPerDeviceKeyboardOsSettingsId, IDS_OS_SETTINGS_TAG_KEYBOARD);
  std::string switch_top_row_key_id = GetSettingsSearchResultId(
      kKeyboardBlockMetaFkeyRewritesOsSettingsId,
      IDS_OS_SETTINGS_TAG_KEYBOARD_BLOCK_META_FKEY_COMBO_REWRITES);
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(result_id));
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(switch_top_row_key_id));
}

// Verify registry updated with regular settings search tags when flag is
// disabled.
TEST_F(DeviceSectionTest, SearchResultChangeBackWithoutFlag) {
  feature_list_.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  device_section_ = std::make_unique<DeviceSection>(
      profile(), search_tag_registry(), printers_manager(), pref_service());

  std::string result_id = GetSubpageSearchResultId(
      kKeyboardOsSettingsId, IDS_OS_SETTINGS_TAG_KEYBOARD);
  std::string switch_top_row_key_id = GetSettingsSearchResultId(
      kKeyboardBlockMetaFkeyRewritesOsSettingsId,
      IDS_OS_SETTINGS_TAG_KEYBOARD_BLOCK_META_FKEY_COMBO_REWRITES);
  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(result_id));
  EXPECT_FALSE(search_tag_registry()->GetTagMetadata(switch_top_row_key_id));
}

}  // namespace ash::settings
