// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const speech::LanguageCode kEnglishLocale = speech::LanguageCode::kEnUs;
const base::TimeDelta kSodaUninstallTime = base::Days(30);

constexpr char kSodaEnglishLanguageInstallationResult[] =
    "SodaInstaller.Language.en-US.InstallationResult";
}  // namespace

namespace speech {

class SodaInstallerImplChromeOSTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    soda_installer_impl_ = std::make_unique<SodaInstallerImplChromeOS>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    soda_installer_impl_->RegisterLocalStatePrefs(pref_service_->registry());
    // Set Dictation pref to true so that SODA will download when calling
    // Init().
    pref_service_->registry()->RegisterBooleanPref(
        ash::prefs::kAccessibilityDictationEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                   true);
    pref_service_->registry()->RegisterBooleanPref(
        ash::prefs::kProjectorCreationFlowEnabled, true);
    pref_service_->registry()->RegisterStringPref(
        ash::prefs::kProjectorCreationFlowLanguage, kUsEnglishLocale);
    pref_service_->registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kUsEnglishLocale);
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    pref_service_.reset();
  }

  SodaInstallerImplChromeOS* GetInstance() {
    return soda_installer_impl_.get();
  }

  bool IsSodaInstalled() {
    return soda_installer_impl_->IsSodaInstalled(kEnglishLocale);
  }

  bool IsLanguageInstalled(LanguageCode language) {
    return soda_installer_impl_->IsLanguageInstalled(language);
  }

  bool IsAnyLanguagePackInstalled() {
    return soda_installer_impl_->IsAnyLanguagePackInstalledForTesting();
  }

  bool IsSodaDownloading() {
    return soda_installer_impl_->IsSodaDownloading(kEnglishLocale);
  }

  void Init() {
    soda_installer_impl_->Init(pref_service_.get(), pref_service_.get());
  }

  void InstallLanguage(const std::string& language) {
    soda_installer_impl_->InstallLanguage(language, pref_service_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetInstallError() {
    fake_dlcservice_client_.set_install_error(dlcservice::kErrorNeedReboot);
  }

  void SetUninstallTimer() {
    soda_installer_impl_->SetUninstallTimer(pref_service_.get(),
                                            pref_service_.get());
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetDictationEnabled(bool enabled) {
    pref_service_->SetManagedPref(ash::prefs::kAccessibilityDictationEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetLiveCaptionEnabled(bool enabled) {
    pref_service_->SetManagedPref(prefs::kLiveCaptionEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetProjectorCreationFlowEnabled(bool enabled) {
    pref_service_->SetManagedPref(ash::prefs::kProjectorCreationFlowEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetSodaInstallerInitialized(bool initialized) {
    soda_installer_impl_->soda_installer_initialized_ = initialized;
  }

  std::unique_ptr<SodaInstallerImplChromeOS> soda_installer_impl_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  ash::FakeDlcserviceClient fake_dlcservice_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SodaInstallerImplChromeOSTest, IsSodaInstalled) {
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(IsSodaInstalled());
  Init();
  ASSERT_FALSE(IsSodaInstalled());
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());

  // SODA binary and english language installation never failed.
  histogram_tester.ExpectBucketCount(kSodaBinaryInstallationResult, 0, 0);
  histogram_tester.ExpectBucketCount(kSodaEnglishLanguageInstallationResult, 0,
                                     0);

  // SODA binary and english language installation succeeded once.
  histogram_tester.ExpectBucketCount(kSodaBinaryInstallationResult, 1, 1);
  histogram_tester.ExpectBucketCount(kSodaEnglishLanguageInstallationResult, 1,
                                     1);
}

TEST_F(SodaInstallerImplChromeOSTest, IsDownloading) {
  ASSERT_FALSE(IsSodaDownloading());
  Init();
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, SubSetCorrect) {
  std::vector<std::string> actual_langs =
      GetInstance()->GetAvailableLanguages();
  auto expected_livecaption_langs =
      GetInstance()->GetLiveCaptionEnabledLanguages();
  EXPECT_THAT(expected_livecaption_langs, ::testing::IsSubsetOf(actual_langs));
}

TEST_F(SodaInstallerImplChromeOSTest, ConchAddOns) {
  base::test::ScopedFeatureList scoped_feature_list_internal;
  scoped_feature_list_internal.InitWithFeatures(
      {::speech::kCrosSodaConchLanguages,
       ::speech::kFeatureManagementCrosSodaConchLanguages},
      {});
  auto expected_livecaption_langs =
      GetInstance()->GetLiveCaptionEnabledLanguages();
  EXPECT_THAT(expected_livecaption_langs,
              ::testing::IsSupersetOf({"da-DK", "nb-NO", "nl-NL", "sv-SE"}));
}

TEST_F(SodaInstallerImplChromeOSTest, ConchInLiveCaptionFullList) {
  base::test::ScopedFeatureList scoped_feature_list_internal;
  scoped_feature_list_internal.InitWithFeatures(
      {::speech::kCrosSodaConchLanguages, ::speech::kCrosExpandSodaLanguages,
       ::speech::kFeatureManagementCrosSodaConchLanguages},
      {});
  soda_installer_impl_.reset();
  soda_installer_impl_ = std::make_unique<SodaInstallerImplChromeOS>();
  std::vector<std::string> enabled_and_available_languages;
  std::vector<base::Value::Dict> available_language_packs;
  {
    auto enabled_languages = GetInstance()->GetLiveCaptionEnabledLanguages();
    auto available_languages = GetInstance()->GetAvailableLanguages();
    auto available_languages_set = std::unordered_set<std::string>(
        available_languages.begin(), available_languages.end());
    for (const auto& enabled_language : enabled_languages) {
      if (available_languages_set.find(enabled_language) !=
          available_languages_set.end()) {
        enabled_and_available_languages.push_back(enabled_language);
      }
    }
  }
  EXPECT_THAT(enabled_and_available_languages,
              ::testing::IsSupersetOf({"da-DK", "nb-NO", "nl-NL", "sv-SE"}));
}

TEST_F(SodaInstallerImplChromeOSTest, MultipleLangsAvailableInExperiment) {
  base::test::ScopedFeatureList scoped_feature_list_internal;
  std::map<std::string, std::string> params;
  params.insert({"available_languages",
                 "it-IT:libsoda-chickenface,ja-JP:libsoda-moo,de-IT:"
                 "incorrectprefix,wr-on:libsoda-wrong-language,de-DE:"});
  scoped_feature_list_internal.InitAndEnableFeatureWithParameters(
      ::speech::kCrosExpandSodaLanguages, params);
  // explicit delete first to make the single instance enforcement happy.
  soda_installer_impl_.reset();
  soda_installer_impl_ = std::make_unique<SodaInstallerImplChromeOS>();
  std::vector<std::string> actual_langs =
      GetInstance()->GetAvailableLanguages();
  EXPECT_THAT(actual_langs,
              ::testing::IsSupersetOf({"ja-JP", "it-IT", "en-US"}));
  EXPECT_TRUE(std::find(actual_langs.begin(), actual_langs.end(), "de-DE") ==
              actual_langs.end());
}

TEST_F(SodaInstallerImplChromeOSTest, IsAnyLanguagePackInstalled) {
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  Init();
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  RunUntilIdle();
  ASSERT_TRUE(IsAnyLanguagePackInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, SodaInstallError) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  SetInstallError();
  Init();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackError) {
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
  SetInstallError();
  Init();
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, InstallSodaForTesting) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());

  // Install just the binary.
  GetInstance()->NotifySodaInstalledForTesting();
  ASSERT_FALSE(IsSodaDownloading());

  // Now install the language pack.
  GetInstance()->NotifySodaInstalledForTesting(kEnglishLocale);
  ASSERT_TRUE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_TRUE(IsLanguageInstalled(kEnglishLocale));
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplChromeOSTest, UninstallSodaForTesting) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  ASSERT_TRUE(IsLanguageInstalled(kEnglishLocale));
  GetInstance()->UninstallSodaForTesting();
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
}

TEST_F(SodaInstallerImplChromeOSTest, SodaProgressForTesting) {
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsSodaDownloading());
  ASSERT_FALSE(IsLanguageInstalled(kEnglishLocale));
  Init();
  GetInstance()->NotifySodaProgressForTesting(50);
  ASSERT_FALSE(IsSodaInstalled());
  ASSERT_FALSE(IsAnyLanguagePackInstalled());
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackForTesting) {
  LanguageCode fr_fr = LanguageCode::kFrFr;
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifySodaProgressForTesting(50, fr_fr);
  ASSERT_TRUE(GetInstance()->IsSodaDownloading(fr_fr));
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifySodaInstalledForTesting(fr_fr);
  ASSERT_TRUE(IsLanguageInstalled(fr_fr));
}

TEST_F(SodaInstallerImplChromeOSTest, LanguagePackErrorForTesting) {
  LanguageCode fr_fr = LanguageCode::kFrFr;
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifySodaProgressForTesting(50, fr_fr);
  ASSERT_TRUE(GetInstance()->IsSodaDownloading(fr_fr));
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  GetInstance()->NotifySodaErrorForTesting(fr_fr);
  ASSERT_FALSE(IsLanguageInstalled(fr_fr));
  ASSERT_FALSE(GetInstance()->IsSodaDownloading(fr_fr));
}

TEST_F(SodaInstallerImplChromeOSTest, UninstallSodaAfterThirtyDays) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off features that use SODA so that the uninstall timer can be set.
  SetDictationEnabled(false);
  SetLiveCaptionEnabled(false);
  SetProjectorCreationFlowEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // If 30 days pass without the uninstall time being pushed, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  // The uninstallation process doesn't start until Init() is called again.
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, ReinstallSoda) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off features that use SODA so that the uninstall timer can be set.
  SetDictationEnabled(false);
  SetLiveCaptionEnabled(false);
  SetProjectorCreationFlowEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // If 30 days pass without the uninstall time being pushed, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  // The uninstallation process doesn't start until Init() is called again.
  Init();
  RunUntilIdle();
  ASSERT_FALSE(IsSodaInstalled());
  // Enable live caption and reinstall SODA.
  SetLiveCaptionEnabled(true);
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

// Tests that SODA stays installed if thirty days pass and a feature using SODA
// is enabled.
TEST_F(SodaInstallerImplChromeOSTest,
       SodaStaysInstalledAfterThirtyDaysIfFeatureEnabled) {
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
  // Turn off Dictation, but keep live caption enabled. This should prevent
  // SODA from automatically uninstalling.
  SetDictationEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());
  Init();
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

}  // namespace speech
