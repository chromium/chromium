// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/shell.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/ash/lobster/mock_lobster_system_state_provider.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {
namespace {
using ::specialized_features::FeatureAccessFailure;
using ::testing::Return;

FakeScannerProfileScopedDelegate* GetFakeScannerProfileScopedDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller.delegate_for_testing()->GetProfileScopedDelegate());
}

}  // namespace

// Test for the search settings page.
class SearchSectionTest : public ChromeAshTestBase {
 public:
  SearchSectionTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}

  ~SearchSectionTest() override = default;

  TestingProfile* profile() { return &profile_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }

 protected:
  void TearDown() override {
    search_section_.reset();
    ChromeAshTestBase::TearDown();
  }

  std::unique_ptr<SearchSection> search_section_;

 private:
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingProfile profile_;
};

TEST_F(SearchSectionTest,
       SearchSectionDoesNotIncludesLobsterSettingsByDefault) {
  search_section_ =
      std::make_unique<SearchSection>(profile(), search_tag_registry());
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-search-section");

  chromeos::test::FakeMagicBoostState magic_boost_state;
  magic_boost_state.SetAvailability(true);

  search_section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_FALSE(html_source->GetLocalizedStrings()
                   ->FindBool("isLobsterSettingsToggleVisible")
                   .value());
}

// MagicBoost availability check requires an async operation. There is a short
// period where `MagicBoostState` returns false for its availability even if a
// user/device is eligible, and magic boost is enabled.
TEST_F(SearchSectionTest,
       QuickAnswersSearchConceptsRemovedIfItBecomesUnavailable) {
  const std::string quick_answers_result_id = base::StrCat(
      {base::ToString(chromeos::settings::mojom::Setting::kQuickAnswersOnOff),
       ",", base::ToString(IDS_OS_SETTINGS_TAG_QUICK_ANSWERS)});

  chromeos::test::FakeMagicBoostState magic_boost_state;
  magic_boost_state.SetAvailability(false);
  magic_boost_state.SetMagicBoostEnabled(true);
  FakeQuickAnswersState quick_answers_state;
  quick_answers_state.SetApplicationLocale("en");
  ASSERT_EQ(QuickAnswersState::FeatureType::kQuickAnswers,
            QuickAnswersState::GetFeatureType())
      << "Current feature type is set to kQuickAnswers. This is simulating the "
         "case where MagicBoost availability check async opearation is not "
         "completed.";

  search_section_ =
      std::make_unique<SearchSection>(profile(), search_tag_registry());

  EXPECT_NE(nullptr,
            search_tag_registry()->GetTagMetadata(quick_answers_result_id))
      << "QuickAnswers tag should be registered as the current feature is set "
         "to kQuickAnswers";

  // Simulate that MagicBoost availability check async operation has been
  // completed.
  magic_boost_state.SetAvailability(true);
  ASSERT_EQ(QuickAnswersState::FeatureType::kHmr,
            QuickAnswersState::GetFeatureType());

  EXPECT_EQ(nullptr,
            search_tag_registry()->GetTagMetadata(quick_answers_result_id))
      << "Quick Answers tag should not be found as the current feature type is "
         "set to kHmr";
}

class SearchSectionTestWithLobsterEnabled : public SearchSectionTest {
 public:
  void SetUp() override {
    SearchSectionTest::SetUp();
    magic_boost_state_.SetAvailability(true);
    feature_list_.InitWithFeatures(
        /*enable_features=*/{ash::features::kLobster,
                             ash::features::kFeatureManagementLobster},
        /*disable_features=*/{});
    AnnotateAccount();

    html_source_ = content::TestWebUIDataSource::Create("test-search-section");
    search_section_ =
        std::make_unique<SearchSection>(profile(), search_tag_registry());
  }

  void TearDown() override {
    magic_boost_state_.RemoveObserver(search_section_.get());
    SearchSectionTest::TearDown();
  }

  content::TestWebUIDataSource* html_source() { return html_source_.get(); }

  void SetUpSystemProviderForLobsterService(
      const ash::LobsterSystemState& system_state) {
    std::unique_ptr<MockLobsterSystemStateProvider> mock_system_state_provider =
        std::make_unique<MockLobsterSystemStateProvider>();
    ON_CALL(*mock_system_state_provider, GetSystemState)
        .WillByDefault(testing::Return(system_state));
    LobsterServiceProvider::GetForProfile(profile())
        ->set_lobster_system_state_provider_for_testing(
            std::move(mock_system_state_provider));
  }

 private:
  void AnnotateAccount() {
    auto* user = fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user->GetAccountId());
    ash::AnnotatedAccountId::Set(profile(), user->GetAccountId());
  }

  base::test::ScopedFeatureList feature_list_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  std::unique_ptr<content::TestWebUIDataSource> html_source_;
  chromeos::test::FakeMagicBoostState magic_boost_state_;
};

TEST_F(SearchSectionTestWithLobsterEnabled,
       DoesNotIncludeLobsterSettingsIfLobsterIsHardBlocked) {
  SetUpSystemProviderForLobsterService(ash::LobsterSystemState(
      ash::LobsterStatus::kBlocked, /*failed_checks=*/{
          ash::LobsterSystemCheck::kInvalidAccountCapabilities}));

  search_section_->AddLoadTimeData(html_source()->GetWebUIDataSource());

  EXPECT_FALSE(html_source()
                   ->GetLocalizedStrings()
                   ->FindBool("isLobsterSettingsToggleVisible")
                   .value());
}

TEST_F(SearchSectionTestWithLobsterEnabled,
       IncludeLobsterSettingsIfLobsterIsSoftBlocked) {
  SetUpSystemProviderForLobsterService(ash::LobsterSystemState(
      ash::LobsterStatus::kBlocked,
      /*failed_checks=*/{ash::LobsterSystemCheck::kSettingsOff}));

  search_section_->AddLoadTimeData(html_source()->GetWebUIDataSource());

  EXPECT_TRUE(html_source()
                  ->GetLocalizedStrings()
                  ->FindBool("isLobsterSettingsToggleVisible")
                  .value());
}

class SearchSectionTestWithScannerEnabled : public ChromeAshTestBase {
 public:
  SearchSectionTestWithScannerEnabled()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}

  ~SearchSectionTestWithScannerEnabled() override = default;

  TestingProfile* profile() { return &profile_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }

 protected:
  void SetUp() override { ChromeAshTestBase::SetUp(); }
  void TearDown() override { ChromeAshTestBase::TearDown(); }

 private:
  // Required to force the scanner controller to be set up with dogfood
  // config.
  base::test::ScopedFeatureList feature_list_{features::kScannerDogfood};
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingProfile profile_;
};

TEST_F(SearchSectionTestWithScannerEnabled,
       DoesNotIncludeScannerSettingsWhenScannerDisabledByAccessChecker) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          FeatureAccessFailure::kDisabledInKioskModeCheckFailed}));

  auto search_section =
      std::make_unique<SearchSection>(profile(), search_tag_registry());
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-search-section");
  chromeos::test::FakeMagicBoostState magic_boost_state;
  magic_boost_state.SetAvailability(true);

  search_section->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_FALSE(html_source->GetLocalizedStrings()
                   ->FindBool("isScannerSettingsToggleVisible")
                   .value());
}

TEST_F(
    SearchSectionTestWithScannerEnabled,
    IncludesScannerSettingsWhenFeatureAccessCheckFailsConsentOrSettingsCheck) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          FeatureAccessFailure::kConsentNotAccepted,
          FeatureAccessFailure::kDisabledInSettings,
      }));

  auto search_section =
      std::make_unique<SearchSection>(profile(), search_tag_registry());
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-search-section");
  chromeos::test::FakeMagicBoostState magic_boost_state;
  magic_boost_state.SetAvailability(true);

  search_section->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_TRUE(html_source->GetLocalizedStrings()
                  ->FindBool("isScannerSettingsToggleVisible")
                  .value());
}

TEST_F(SearchSectionTestWithScannerEnabled,
       IncludeScannerSettingsWhenNoChecksFail) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  auto search_section =
      std::make_unique<SearchSection>(profile(), search_tag_registry());
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-search-section");
  chromeos::test::FakeMagicBoostState magic_boost_state;
  magic_boost_state.SetAvailability(true);

  search_section->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_TRUE(html_source->GetLocalizedStrings()
                  ->FindBool("isScannerSettingsToggleVisible")
                  .value());
}

}  // namespace ash::settings
