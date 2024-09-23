// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/inputs_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"

namespace ash::settings {

namespace {

constexpr OsSettingsIdentifier kShowOrcaSettingsId = {
    .setting = chromeos::settings::mojom::Setting::kShowOrca};

std::string GetSettingsSearchResultId(OsSettingsIdentifier id, int message_id) {
  std::stringstream ss;
  ss << id.setting << "," << message_id;
  return ss.str();
}

}  // namespace

// Test for the inputs settings page.
class InputsSectionTest : public ChromeAshTestBase {
 public:
  InputsSectionTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}

  ~InputsSectionTest() override = default;

  TestingProfile* profile() { return &profile_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }

  std::unique_ptr<InputsSection> inputs_section_;

 protected:
  void SetUp() override {
    pref_service()->registry()->RegisterBooleanPref(
        prefs::kEmojiSuggestionEnterpriseAllowed, true);
    pref_service()->registry()->RegisterBooleanPref(
        spellcheck::prefs::kSpellCheckEnable, true);

    ChromeAshTestBase::SetUp();
  }
  void TearDown() override {
    inputs_section_.reset();
    ChromeAshTestBase::TearDown();
  }

 private:
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingPrefServiceSimple pref_service_;
  TestingProfile profile_;
};

TEST_F(InputsSectionTest,
       InputsSectionShouldIncludeHelpMeWriteSettingsWhenMagicBoostIsDisabled) {
  base::test::ScopedFeatureList feature_list;

  // Note that `kMahi` is associated with the Magic Boost feature.
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {
          chromeos::features::kFeatureManagementOrca,
      },
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities,
                             chromeos::features::kFeatureManagementMahi,
                             chromeos::features::kMahi});

  auto mock_geolocation_provider =
      std::make_unique<input_method::EditorGeolocationMockProvider>("us");
  input_method::EditorMediator editor_mediator(
      profile(), std::move(mock_geolocation_provider));
  inputs_section_ = std::make_unique<InputsSection>(
      profile(), search_tag_registry(), pref_service(), &editor_mediator);
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-inputs-section");

  inputs_section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_TRUE(
      html_source->GetLocalizedStrings()->FindBool("allowOrca").value());
}

TEST_F(InputsSectionTest,
       InputsSectionShouldNotHaveHelpMeWriteSettingsWhenMagicBoostIsEnabled) {
  base::test::ScopedFeatureList feature_list;

  // Note that `kMahi` is associated with the Magic Boost feature.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kFeatureManagementOrca,
                            chromeos::features::kFeatureManagementMahi,
                            chromeos::features::kMahi},
      /*disabled_features=*/{
          ash::features::kOrcaUseAccountCapabilities,
      });

  chromeos::test::FakeMagicBoostState magic_boost_state;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kMahiRestrictionsOverride);

  auto mock_geolocation_provider =
      std::make_unique<input_method::EditorGeolocationMockProvider>("us");
  input_method::EditorMediator editor_mediator(
      profile(), std::move(mock_geolocation_provider));
  inputs_section_ = std::make_unique<InputsSection>(
      profile(), search_tag_registry(), pref_service(), &editor_mediator);
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-inputs-section");

  inputs_section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_FALSE(
      html_source->GetLocalizedStrings()->FindBool("allowOrca").value());
}

TEST_F(InputsSectionTest, SearchResultShouldIncludeHelpMeWrite) {
  base::test::ScopedFeatureList feature_list;

  // Note that `kMahi` is associated with the Magic Boost feature.
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {
          chromeos::features::kFeatureManagementOrca,
      },
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities,
                             chromeos::features::kFeatureManagementMahi,
                             chromeos::features::kMahi});

  auto mock_geolocation_provider =
      std::make_unique<input_method::EditorGeolocationMockProvider>("us");
  input_method::EditorMediator editor_mediator(
      profile(), std::move(mock_geolocation_provider));
  inputs_section_ = std::make_unique<InputsSection>(
      profile(), search_tag_registry(), pref_service(), &editor_mediator);

  std::string result_id = GetSettingsSearchResultId(
      kShowOrcaSettingsId,
      IDS_OS_SETTINGS_TAG_LANGUAGES_HELP_ME_WRITE_SUGGESTIONS);

  EXPECT_TRUE(search_tag_registry()->GetTagMetadata(result_id));
}

}  // namespace ash::settings
