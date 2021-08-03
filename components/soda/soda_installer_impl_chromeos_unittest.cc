// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const speech::LanguageCode kEnglishLocale = speech::LanguageCode::kEnUs;
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
                                                   false);

    chromeos::DBusThreadManager::Initialize();
    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    pref_service_.reset();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::DlcserviceClient::Shutdown();
  }

  SodaInstallerImplChromeOS* GetInstance() {
    return soda_installer_impl_.get();
  }

  bool IsSodaInstalled() {
    return soda_installer_impl_->IsSodaInstalled(speech::LanguageCode::kEnUs);
  }

  bool IsLanguageInstalled(speech::LanguageCode language) {
    return soda_installer_impl_->IsLanguageInstalled(language);
  }

  bool IsAnyLanguagePackInstalled() {
    return soda_installer_impl_->IsAnyLanguagePackInstalled();
  }

  bool IsSodaDownloading() {
    return soda_installer_impl_->IsSodaDownloading(speech::LanguageCode::kEnUs);
  }

  void Init() {
    soda_installer_impl_->Init(pref_service_.get(), pref_service_.get());
  }

  void InstallLanguage(const std::string& language) {
    soda_installer_impl_->InstallLanguage(language, pref_service_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetInstallError() {
    fake_dlcservice_client_->set_install_error(dlcservice::kErrorNeedReboot);
  }

 private:
  std::unique_ptr<SodaInstallerImplChromeOS> soda_installer_impl_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SodaInstallerImplChromeOSTest, IsSodaInstalled) {
  ASSERT_FALSE(IsSodaInstalled());
  Init();
  ASSERT_FALSE(IsSodaInstalled());
  RunUntilIdle();
  ASSERT_TRUE(IsSodaInstalled());
}

TEST_F(SodaInstallerImplChromeOSTest, IsDownloading) {
  ASSERT_FALSE(IsSodaDownloading());
  Init();
  ASSERT_TRUE(IsSodaDownloading());
  RunUntilIdle();
  ASSERT_FALSE(IsSodaDownloading());
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
  GetInstance()->NotifySodaInstalledForTesting();
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

}  // namespace speech
