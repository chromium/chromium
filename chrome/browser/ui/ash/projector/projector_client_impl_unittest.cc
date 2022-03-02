// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kFirstSpeechResult[] = "the brown fox";
const char kSecondSpeechResult[] = "the brown fox jumped over the lazy dog";

const char kEnglishLocale[] = "en-US";

inline void SetLocale(const std::string& locale) {
  g_browser_process->SetApplicationLocale(locale);
}

// A mocked version instance of SodaInstaller for testing purposes.
class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller() = default;
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override = default;

  MOCK_CONST_METHOD0(GetSodaBinaryPath, base::FilePath());
  MOCK_CONST_METHOD1(GetLanguagePath, base::FilePath(const std::string&));
  MOCK_METHOD2(InstallLanguage, void(const std::string&, PrefService*));
  MOCK_CONST_METHOD0(GetAvailableLanguages, std::vector<std::string>());
  MOCK_METHOD1(InstallSoda, void(PrefService*));
  MOCK_METHOD1(UninstallSoda, void(PrefService*));
};

}  // namespace

class ProjectorClientImplUnitTest : public testing::Test {
 public:
  ProjectorClientImplUnitTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kProjector, features::kOnDeviceSpeechRecognition}, {});
  }

  ProjectorClientImplUnitTest(const ProjectorClientImplUnitTest&) = delete;
  ProjectorClientImplUnitTest& operator=(const ProjectorClientImplUnitTest&) =
      delete;
  ~ProjectorClientImplUnitTest() override = default;

  Profile* profile() { return testing_profile_; }

  MockProjectorController& projector_controller() {
    return projector_controller_;
  }

  ProjectorClient* client() { return projector_client_.get(); }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ = ProfileManager::GetPrimaryUserProfile();
    ASSERT_TRUE(testing_profile_);

    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&ProjectorClientImplUnitTest::
                                    CreateTestSpeechRecognitionService,
                                base::Unretained(this)));
    SetLocale(kEnglishLocale);
    soda_installer_ = std::make_unique<MockSodaInstaller>();
    soda_installer_->NotifySodaInstalledForTesting();
    soda_installer_->NotifySodaInstalledForTesting(speech::LanguageCode::kEnUs);
    projector_client_ =
        std::make_unique<ProjectorClientImpl>(&projector_controller_);
  }

  std::unique_ptr<KeyedService> CreateTestSpeechRecognitionService(
      content::BrowserContext* context) {
    std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
        std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  void SendSpeechResult(const char* result, bool is_final) {
    EXPECT_TRUE(fake_service_->is_capturing_audio());
    base::RunLoop loop;
    fake_service_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(result, is_final));
    loop.RunUntilIdle();
  }

  void SendTranscriptionError() {
    EXPECT_TRUE(fake_service_->is_capturing_audio());
    base::RunLoop loop;
    fake_service_->SendSpeechRecognitionError();
    loop.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  Profile* testing_profile_ = nullptr;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  MockProjectorController projector_controller_;
  std::unique_ptr<ProjectorClient> projector_client_;
  std::unique_ptr<MockSodaInstaller> soda_installer_;
  speech::FakeSpeechRecognitionService* fake_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorClientImplUnitTest, SpeechRecognitionResults) {
  client()->StartSpeechRecognition();
  fake_service_->WaitForRecognitionStarted();

  EXPECT_CALL(projector_controller(),
              OnTranscription(
                  media::SpeechRecognitionResult(kFirstSpeechResult, false)));
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);

  EXPECT_CALL(projector_controller(),
              OnTranscription(
                  media::SpeechRecognitionResult(kSecondSpeechResult, false)));
  SendSpeechResult(kSecondSpeechResult, /*is_final=*/false);

  EXPECT_CALL(projector_controller(), OnTranscriptionError());
  SendTranscriptionError();
}

}  // namespace ash
