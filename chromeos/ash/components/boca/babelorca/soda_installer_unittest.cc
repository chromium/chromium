// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/soda_installer.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/boca/babelorca/soda_testing_utils.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {

namespace {
constexpr char kAlternativeLanguage[] = "de-DE";
constexpr char kDefaultLanguage[] = "en-US";
constexpr char kBadLanguage[] = "unkown language";
constexpr char kTeacherSetting[] = "teacher";
}  // namespace

class BabelOrcaSodaInstallerTest : public testing::Test {
 public:
  BabelOrcaSodaInstallerTest() {
    feature_list_.InitWithFeatures({ash::features::kOnDeviceSpeechRecognition},
                                   {});
  }
  ~BabelOrcaSodaInstallerTest() override = default;
  BabelOrcaSodaInstallerTest(const BabelOrcaSodaInstallerTest&) = delete;
  BabelOrcaSodaInstallerTest operator=(const BabelOrcaSodaInstallerTest&) =
      delete;

  void SetUp() override {
    installer_under_test_ = std::make_unique<SodaInstaller>(
        &global_prefs_, &profile_prefs_, kDefaultLanguage);
    speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
    speech::SodaInstaller::GetInstance()->RegisterLocalStatePrefs(
        global_prefs_.registry());

    RegisterSodaPrefsForTesting(profile_prefs_.registry());
    // this is set here because the SessionManager test registers this
    // preference and both tests depend on the RegisterSodaPrefsForTesting
    // method.
    profile_prefs_.registry()->RegisterStringPref(
        ash::prefs::kClassManagementToolsAvailabilitySetting, kTeacherSetting);

    EXPECT_CALL(soda_installer_, GetAvailableLanguages)
        .WillRepeatedly(testing::Return(available_languages_));
  }

  std::vector<std::string> available_languages_ = {{kDefaultLanguage}};
  MockSodaInstaller soda_installer_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple global_prefs_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<SodaInstaller> installer_under_test_;
};

TEST_F(BabelOrcaSodaInstallerTest, UninstalledDefaultStatus) {
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kUninstalled);
}

TEST_F(BabelOrcaSodaInstallerTest, CallsBackImmediatelyIfPackInstalled) {
  std::optional<SodaInstaller::InstallationStatus> speech_recognition_available;
  // we expect two calls to InstallLanguage, once when during Init the
  // installer installs the language associated with live capiton.
  // then when the event handler calls InstallLanguage for the
  // language associated with BabelOrca.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));

  EXPECT_CALL(soda_installer_, InstallSoda).Times(0);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(0);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kReady);

  ASSERT_TRUE(speech_recognition_available.has_value());
  ASSERT_EQ(speech_recognition_available.value(),
            SodaInstaller::InstallationStatus::kReady);
}

TEST_F(BabelOrcaSodaInstallerTest, InstallsSodaIfNeeded) {
  std::optional<SodaInstaller::InstallationStatus> speech_recognition_available;
  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kReady);

  ASSERT_TRUE(speech_recognition_available.has_value());
  ASSERT_EQ(speech_recognition_available.value(),
            SodaInstaller::InstallationStatus::kReady);
}

TEST_F(BabelOrcaSodaInstallerTest, ReportsSodaInstallFailure) {
  std::optional<SodaInstaller::InstallationStatus> speech_recognition_available;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kNone);
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstallationFailure);

  ASSERT_TRUE(speech_recognition_available.has_value());
  ASSERT_EQ(speech_recognition_available.value(),
            SodaInstaller::InstallationStatus::kInstallationFailure);
}

TEST_F(BabelOrcaSodaInstallerTest, ReportsSodaLanguageInstallFailure) {
  std::optional<SodaInstaller::InstallationStatus> speech_recognition_available;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstallationFailure);

  ASSERT_TRUE(speech_recognition_available.has_value());
  ASSERT_EQ(speech_recognition_available.value(),
            SodaInstaller::InstallationStatus::kInstallationFailure);
}

TEST_F(BabelOrcaSodaInstallerTest, IgnoresOtherLanguageFailures) {
  // If the failure is for the relevant language then this will
  // be flipped by the end of the test, so we expect it to have
  // the same value when we assert.
  bool availability_callback_invoked = false;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&availability_callback_invoked](
          SodaInstaller::InstallationStatus available) {
        availability_callback_invoked = true;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kAlternativeLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);

  // The callback should not have been called.
  ASSERT_FALSE(availability_callback_invoked);
}

TEST_F(BabelOrcaSodaInstallerTest, IgnoresOtherLanguageInstalls) {
  bool availability_callback_invoked = false;
  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&availability_callback_invoked](
          SodaInstaller::InstallationStatus available) {
        availability_callback_invoked = true;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kAlternativeLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);

  // The callback should not have been called.
  ASSERT_FALSE(availability_callback_invoked);
}

// Calls to these observer methods without the callback set during
// StartSpeechRecognition are no-ops.  These ensure that calling the observer
// methods when not waiting for an install won't crash if they're invoked by the
// installer.
TEST_F(BabelOrcaSodaInstallerTest, DoesNotCrashIfNoCallbackInstall) {
  installer_under_test_->OnSodaInstalled(
      speech::GetLanguageCode(kDefaultLanguage));
}

TEST_F(BabelOrcaSodaInstallerTest, DoesNotCrashIfNoCallbackError) {
  installer_under_test_->OnSodaInstallError(
      speech::GetLanguageCode(kDefaultLanguage),
      speech::SodaInstaller::ErrorCode::kUnspecifiedError);
}

TEST_F(BabelOrcaSodaInstallerTest, HandlesMultipleInstallationObservers) {
  std::optional<SodaInstaller::InstallationStatus>
      speech_recognition_available_one;
  std::optional<SodaInstaller::InstallationStatus>
      speech_recognition_available_two;
  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available_one](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available_one = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available_two](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available_two = available;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kReady);

  ASSERT_TRUE(speech_recognition_available_one.has_value());
  ASSERT_EQ(speech_recognition_available_one.value(),
            SodaInstaller::InstallationStatus::kReady);
  ASSERT_TRUE(speech_recognition_available_one.has_value());
  ASSERT_EQ(speech_recognition_available_two.value(),
            SodaInstaller::InstallationStatus::kReady);
}

TEST_F(BabelOrcaSodaInstallerTest,
       HandlesMultipleInstallationObserversOnFailure) {
  std::optional<SodaInstaller::InstallationStatus>
      speech_recognition_available_one;
  std::optional<SodaInstaller::InstallationStatus>
      speech_recognition_available_two;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available_one](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available_one = available;
      }));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstalling);
  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available_two](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available_two = available;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kInstallationFailure);

  ASSERT_TRUE(speech_recognition_available_one.has_value());
  ASSERT_EQ(speech_recognition_available_one.value(),
            SodaInstaller::InstallationStatus::kInstallationFailure);
  ASSERT_TRUE(speech_recognition_available_two.has_value());
  ASSERT_EQ(speech_recognition_available_two.value(),
            SodaInstaller::InstallationStatus::kInstallationFailure);
}

TEST_F(BabelOrcaSodaInstallerTest, GetStatusCatchesBadLanguage) {
  installer_under_test_ = std::make_unique<SodaInstaller>(
      &global_prefs_, &profile_prefs_, kBadLanguage);
  EXPECT_EQ(installer_under_test_->GetStatus(),
            SodaInstaller::InstallationStatus::kLanguageUnavailable);
}

TEST_F(BabelOrcaSodaInstallerTest, InstallLanguageCatchesBadLanguage) {
  std::optional<SodaInstaller::InstallationStatus> speech_recognition_available;
  installer_under_test_ = std::make_unique<SodaInstaller>(
      &global_prefs_, &profile_prefs_, kBadLanguage);
  installer_under_test_->InstallSoda(base::BindLambdaForTesting(
      [&speech_recognition_available](
          SodaInstaller::InstallationStatus available) {
        speech_recognition_available = available;
      }));
  EXPECT_EQ(speech_recognition_available,
            SodaInstaller::InstallationStatus::kLanguageUnavailable);
}
}  // namespace ash::babelorca
