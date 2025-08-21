// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/caption_style.h"
#include "ui/views/layout/layout_provider.h"

namespace captions {
namespace {

class MockCaptionControllerDelgate : public CaptionControllerBase::Delegate {
 public:
  MockCaptionControllerDelgate() = default;
  ~MockCaptionControllerDelgate() override = default;

  MOCK_METHOD(std::unique_ptr<CaptionBubbleController>,
              CreateCaptionBubbleController,
              (CaptionBubbleSettings*,
               const std::string&,
               std::unique_ptr<TranslationViewWrapperBase>),
              (override));

  void AddCaptionStyleObserver(ui::NativeThemeObserver*) override {}

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver*) override {}
};

void RegisterStylePrefs(TestingPrefServiceSimple* pref_service) {
  const std::string kCaptionsTextSize = "20%";
  const std::string kCaptionsTextFont = "aerial";
  const std::string kCaptionsTextColor = "255,99,71";
  const std::string kCaptionsBackgroundColor = "90,255,50";
  const std::string kCaptionsTextShadow = "10px";
  constexpr int kCaptionsTextOpacity = 50;
  constexpr int kCaptionsBackgroundOpacity = 30;

  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextSize, kCaptionsTextSize);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextFont, kCaptionsTextFont);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextColor, kCaptionsTextColor);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsTextOpacity, kCaptionsTextOpacity);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsBackgroundColor, kCaptionsBackgroundColor);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextShadow, kCaptionsTextShadow);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsBackgroundOpacity,
      kCaptionsBackgroundOpacity);
}

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
    RegisterStylePrefs(&testing_pref_service_);

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
  speech::MockSodaInstaller soda_installer_;
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

  auto mock_delegate = std::make_unique<MockCaptionControllerDelgate>();
  auto* mock_delegate_ptr = mock_delegate.get();

  EXPECT_CALL(*mock_delegate_ptr, CreateCaptionBubbleController).Times(1);
  LiveCaptionController controller_under_test = LiveCaptionController(
      &testing_pref_service_, &testing_pref_service_, speech::kUsEnglishLocale,
      /*browser_context=*/nullptr, std::move(mock_delegate));
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

  auto mock_delegate = std::make_unique<MockCaptionControllerDelgate>();
  auto* mock_delegate_ptr = mock_delegate.get();
  LiveCaptionController controller_under_test = LiveCaptionController(
      &testing_pref_service_, &testing_pref_service_, speech::kUsEnglishLocale,
      /*browser_context=*/nullptr, std::move(mock_delegate));

  EXPECT_CALL(*mock_delegate_ptr, CreateCaptionBubbleController).Times(1);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(speech::kUsEnglishLocale));
  NotifySodaBinaryInstalled();
}

}  // namespace
}  // namespace captions
