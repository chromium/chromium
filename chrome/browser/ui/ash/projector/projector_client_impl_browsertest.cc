// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/projector_app/projector_app_constants.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kFirstSpeechResult[] = "the brown fox";
const char kSecondSpeechResult[] = "the brown fox jumped over the lazy dog";

}  // namespace

class ASH_PUBLIC_EXPORT MockProjectorController : public ProjectorController {
 public:
  MockProjectorController() = default;

  MockProjectorController(const MockProjectorController&) = delete;
  MockProjectorController& operator=(const MockProjectorController&) = delete;
  ~MockProjectorController() override = default;

  // ProjectorController:
  MOCK_METHOD1(SetClient, void(ProjectorClient* client));
  MOCK_METHOD1(OnSpeechRecognitionAvailable, void(bool available));
  MOCK_METHOD1(OnTranscription,
               void(const media::SpeechRecognitionResult& result));
  MOCK_METHOD0(OnTranscriptionError, void());
  MOCK_METHOD1(SetProjectorToolsVisible, void(bool is_visible));
  MOCK_CONST_METHOD0(IsEligible, bool());
};

class ProjectorClientTest : public InProcessBrowserTest {
 public:
  ProjectorClientTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kProjector, features::kOnDeviceSpeechRecognition}, {});
  }

  ~ProjectorClientTest() override = default;
  ProjectorClientTest(const ProjectorClientTest&) = delete;
  ProjectorClientTest& operator=(const ProjectorClientTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();

    scoped_resetter_ =
        std::make_unique<ProjectorController::ScopedInstanceResetterForTest>();
    controller_ = std::make_unique<MockProjectorController>();
    client_ = std::make_unique<ProjectorClientImpl>(controller_.get());

    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(
                &ProjectorClientTest::CreateTestSpeechRecognitionService,
                base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    client_.reset();
    controller_.reset();
    scoped_resetter_.reset();
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

  // This test helper verifies that navigating to the |url| doesn't result in a
  // 404 error.
  void VerifyUrlValid(const char* url) {
    GURL gurl(url);
    EXPECT_TRUE(gurl.is_valid());
    ui_test_utils::NavigateToURL(browser(), gurl);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL);
  }

 protected:
  std::unique_ptr<ProjectorController::ScopedInstanceResetterForTest>
      scoped_resetter_;
  std::unique_ptr<MockProjectorController> controller_;
  std::unique_ptr<ProjectorClient> client_;
  speech::FakeSpeechRecognitionService* fake_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, ShowOrCloseSelfieCamTest) {
  EXPECT_FALSE(client_->IsSelfieCamVisible());
  client_->ShowSelfieCam();
  EXPECT_TRUE(client_->IsSelfieCamVisible());
  client_->CloseSelfieCam();
  EXPECT_FALSE(client_->IsSelfieCamVisible());
}

// This test verifies that the selfie cam WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(ProjectorClientTest, SelfieCamUrlValid) {
  VerifyUrlValid(chromeos::kChromeUITrustedProjectorSelfieCamUrl);
}

// This test verifies that the Projector app WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(ProjectorClientTest, PlayerUrlValid) {
  VerifyUrlValid(chromeos::kChromeUITrustedProjectorPlayerUrl);
}

// TODO(crbug/1199396): Add a test to verify the selfie cam turns off when the
// device goes inactive.

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, SpeechRecognitionResults) {
  client_->StartSpeechRecognition();
  fake_service_->WaitForRecognitionStarted();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_, OnTranscription(media::SpeechRecognitionResult(
                                kFirstSpeechResult, false)));
  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_, OnTranscription(media::SpeechRecognitionResult(
                                kSecondSpeechResult, false)));
  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_, OnTranscriptionError());
  SendTranscriptionError();
}

}  // namespace ash
