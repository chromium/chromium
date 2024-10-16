// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_controller.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"

namespace captions {

class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller() = default;
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override = default;

  MOCK_METHOD(base::FilePath, GetSodaBinaryPath, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetLanguagePath,
              (const std::string&),
              (const, override));
  MOCK_METHOD(void,
              InstallLanguage,
              (const std::string&, PrefService*),
              (override));
  MOCK_METHOD(void,
              UninstallLanguage,
              (const std::string&, PrefService*),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAvailableLanguages,
              (),
              (const, override));
  MOCK_METHOD(void, InstallSoda, (PrefService*), (override));
  MOCK_METHOD(void, UninstallSoda, (PrefService*), (override));
  MOCK_METHOD(void, Init, (PrefService*, PrefService*), (override));
};

class LiveCaptionControllerTest : public testing::Test {
 public:
  LiveCaptionControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }
  ~LiveCaptionControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    LiveCaptionController::RegisterProfilePrefs(
        static_cast<user_prefs::PrefRegistrySyncable*>(
            testing_pref_service_.registry()));

    // Set up soda Installer
    soda_installer_.NeverDownloadSodaForTesting();

    ON_CALL(soda_installer_, Init).WillByDefault(testing::Return());
  }

  void SetNonEmptyFilePathForSoda() {
    auto non_empty_filepath = base::FilePath("any/path/thats/not/empty");
    ASSERT_FALSE(non_empty_filepath.empty());
    ON_CALL(soda_installer_, GetLanguagePath)
        .WillByDefault(testing::Return(non_empty_filepath));
  }

  void NotifySodaBinaryInstalled() {
    // Calling this method without parameters represents an installation
    // of the SODA binary rather than any given language pack.  Listeners
    // will not be notified of language pack installations until this
    // method is invoked.
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  bool HasBubbleController(LiveCaptionController& controller) {
    return controller.caption_bubble_controller_for_testing() != nullptr;
  }

  TestingPrefServiceSimple testing_pref_service_;
  MockSodaInstaller soda_installer_;
  views::LayoutProvider layout_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that profile prefs are registered correctly in
// this unit test.
TEST_F(LiveCaptionControllerTest, RegisterProfilePrefsCorrect) {
  EXPECT_FALSE(
      testing_pref_service_.GetBoolean(prefs::kLiveCaptionBubbleExpanded));
  EXPECT_FALSE(testing_pref_service_.GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_FALSE(
      testing_pref_service_.GetBoolean(prefs::kLiveCaptionMaskOffensiveWords));
  EXPECT_EQ(testing_pref_service_.GetString(prefs::kLiveCaptionLanguageCode),
            speech::kUsEnglishLocale);

  // Babel Orca flags are only registered on ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(testing_pref_service_.GetBoolean(
      prefs::kLiveCaptionUserMicrophoneEnabled));
  EXPECT_EQ(testing_pref_service_.GetString(
                prefs::kUserMicrophoneCaptionLanguageCode),
            speech::kUsEnglishLocale);
#endif
}

// Tests that the LiveCaptionController starts live caption
// on construction given the correct preconditions are met.
TEST_F(LiveCaptionControllerTest, StartsLiveCaptionOnCtorIfEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing_pref_service_.SetBoolean(prefs::kLiveCaptionEnabled, true);
  // This actually doesn't notify anyone as there are no listeners at this
  // point, however it will update the state of the SODA Installer such that the
  // default language will appear installed to the controller.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(speech::kUsEnglishLocale));
  NotifySodaBinaryInstalled();

  SetNonEmptyFilePathForSoda();

  EXPECT_CALL(soda_installer_, GetLanguagePath);

  base::RunLoop run_loop;
  LiveCaptionController controller_under_test = LiveCaptionController(
      &testing_pref_service_, &testing_pref_service_, speech::kUsEnglishLocale,
      /*browser_context=*/nullptr, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that the LiveCaptionController starts live caption
// after soda installation if preference was set to true.
TEST_F(LiveCaptionControllerTest,
       StartsIfEnabledOnCtorAndSodaNeedsInstallation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing_pref_service_.SetBoolean(prefs::kLiveCaptionEnabled, true);

  SetNonEmptyFilePathForSoda();

  EXPECT_CALL(soda_installer_, GetLanguagePath);
  EXPECT_CALL(soda_installer_, Init);

  base::RunLoop run_loop;
  LiveCaptionController controller_under_test = LiveCaptionController(
      &testing_pref_service_, &testing_pref_service_, speech::kUsEnglishLocale,
      /*browser_context=*/nullptr, run_loop.QuitClosure());

  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(speech::kUsEnglishLocale));
  NotifySodaBinaryInstalled();

  run_loop.Run();
}

}  // namespace captions
