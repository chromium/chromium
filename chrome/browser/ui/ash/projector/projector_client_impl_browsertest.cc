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
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ =
        base::BindRepeating(&ProjectorClientTest::CreateDriveIntegrationService,
                            base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();

    scoped_resetter_ =
        std::make_unique<ProjectorController::ScopedInstanceResetterForTest>();
    controller_ = std::make_unique<ash::MockProjectorController>();
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
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL);
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    // The integration service is owned by `KeyedServiceFactory`.
    auto* integration_service = new drive::DriveIntegrationService(
        profile, /*test_mount_point_name=*/std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

 protected:
  std::unique_ptr<ProjectorController::ScopedInstanceResetterForTest>
      scoped_resetter_;
  std::unique_ptr<ash::MockProjectorController> controller_;
  std::unique_ptr<ProjectorClient> client_;
  speech::FakeSpeechRecognitionService* fake_service_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;

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
  VerifyUrlValid(kChromeUITrustedProjectorSelfieCamUrl);
}

// This test verifies that the Projector app WebUI URL is valid.
IN_PROC_BROWSER_TEST_F(ProjectorClientTest, AppUrlValid) {
  VerifyUrlValid(kChromeUITrustedProjectorAppUrl);
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

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, GetDriveFsMountPointPath) {
  ASSERT_TRUE(client_->IsDriveFsMounted());

  base::FilePath mounted_path;
  ASSERT_TRUE(client_->GetDriveFsMountPointPath(&mounted_path));
  ASSERT_EQ(browser()->profile()->GetPath().Append("drivefs"), mounted_path);
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, OpenProjectorApp) {
  auto* profile = browser()->profile();
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client_->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify that Projector App is opened.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);
}

}  // namespace ash
