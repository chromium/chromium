// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/soda_installer.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
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
constexpr char kTeacherSetting[] = "teacher";
}  // namespace

class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller() = default;
  ~MockSodaInstaller() override = default;

  MOCK_METHOD(base::FilePath, GetSodaBinaryPath, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetLanguagePath,
              (const std::string& language),
              (const, override));
  MOCK_METHOD(void,
              InstallLanguage,
              (const std::string& language, PrefService* global_prefs),
              (override));
  MOCK_METHOD(void,
              UninstallLanguage,
              (const std::string& language, PrefService* global_prefs),
              (override));
  MOCK_METHOD(void, InstallSoda, (PrefService * global_prefs), (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAvailableLanguages,
              (),
              (const, override));

 protected:
  MOCK_METHOD(void, UninstallSoda, (PrefService * global_prefs), (override));
};

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

    // During Soda installer's init method it checks these prefs, if none are
    // enabled and `kClassManagementToolsAvailabilitySetting` is not set to
    // teacher then it will not install SODA.  By setting everything except for
    // the classroom management setting to false we ensure that SODA will
    // install even if just school tools teacher is enabled on the current
    // profile.
    profile_prefs_.registry()->RegisterBooleanPref(::prefs::kLiveCaptionEnabled,
                                                   false);
    profile_prefs_.registry()->RegisterBooleanPref(
        ash::prefs::kAccessibilityDictationEnabled, false);
    profile_prefs_.registry()->RegisterBooleanPref(
        ash::prefs::kProjectorCreationFlowEnabled, false);
    profile_prefs_.registry()->RegisterStringPref(
        ash::prefs::kClassManagementToolsAvailabilitySetting, kTeacherSetting);
  }

  MockSodaInstaller soda_installer_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple global_prefs_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<SodaInstaller> installer_under_test_;
};

TEST_F(BabelOrcaSodaInstallerTest, CallsBackImmediatelyIfPackInstalled) {
  bool speech_recognition_available = false;

  // This first call fakes the binary installation, which is necessary for the
  // installer to report the installed language correctly.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));

  EXPECT_CALL(soda_installer_, InstallSoda).Times(0);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(0);

  installer_under_test_->GetAvailabilityOrInstall(base::BindLambdaForTesting(
      [&speech_recognition_available](bool available) {
        speech_recognition_available = available;
      }));

  ASSERT_TRUE(speech_recognition_available);
}

TEST_F(BabelOrcaSodaInstallerTest, InstallsSodaIfNeeded) {
  bool speech_recognition_available = false;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  // we expect two calls to InstallLanguage, once when during Init the
  // installer installs the language associated with live capiton.
  // then when the event handler calls InstallLanguage for the
  // language associated with BabelOrca.
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->GetAvailabilityOrInstall(base::BindLambdaForTesting(
      [&speech_recognition_available](bool available) {
        speech_recognition_available = available;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kDefaultLanguage));

  ASSERT_TRUE(speech_recognition_available);
}

TEST_F(BabelOrcaSodaInstallerTest, ReportsSodaInstallFailure) {
  // This should be set to false by the end of the test.
  bool speech_recognition_available = true;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->GetAvailabilityOrInstall(base::BindLambdaForTesting(
      [&speech_recognition_available](bool available) {
        speech_recognition_available = available;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kDefaultLanguage));

  ASSERT_FALSE(speech_recognition_available);
}

TEST_F(BabelOrcaSodaInstallerTest, IgnoresOtherLanguageFailures) {
  // If the failure is for the relevant language then this will
  // be flipped by the end of the test, so we expect it to have
  // the same value when we assert.
  bool availability_callback_invoked = false;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->GetAvailabilityOrInstall(base::BindLambdaForTesting(
      [&availability_callback_invoked](bool available) {
        availability_callback_invoked = true;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::GetLanguageCode(kAlternativeLanguage));

  // The callback should not have been called.
  ASSERT_FALSE(availability_callback_invoked);
}

TEST_F(BabelOrcaSodaInstallerTest, IgnoresOtherLanguageInstalls) {
  bool availability_callback_invoked = false;

  EXPECT_CALL(soda_installer_, InstallSoda).Times(1);
  EXPECT_CALL(soda_installer_, InstallLanguage).Times(2);

  installer_under_test_->GetAvailabilityOrInstall(base::BindLambdaForTesting(
      [&availability_callback_invoked](bool available) {
        availability_callback_invoked = true;
      }));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kAlternativeLanguage));

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
}  // namespace ash::babelorca
