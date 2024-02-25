// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_controller_driver_helper.h"

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using password_manager::PasswordManager;
using testing::Return;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
};

class PasswordControllerDriverHelperTest : public PlatformTest {
 public:
  PasswordControllerDriverHelperTest() : PlatformTest() {
    password_controller_helper_ =
        [[PasswordControllerDriverHelper alloc] initWithWebState:&web_state_];

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));

    password_controller_ = OCMStrictClassMock([SharedPasswordController class]);

    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, &password_manager_);
  }

 protected:
  web::FakeWebState web_state_;
  id password_controller_;
  PasswordControllerDriverHelper* password_controller_helper_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  testing::StrictMock<MockPasswordManagerClient> password_manager_client_;
  PasswordManager password_manager_ =
      PasswordManager(&password_manager_client_);
};

// Tests that the driver can be retrieved.
TEST_F(PasswordControllerDriverHelperTest, PasswordManagerDriver) {
  auto web_frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  IOSPasswordManagerDriver* driver =
      [password_controller_helper_ PasswordManagerDriver:frame];
  ASSERT_TRUE(driver != nullptr);
}

// Tests that the password generation helper can be retrieved.
TEST_F(PasswordControllerDriverHelperTest, PasswordGenerationHelper) {
  auto web_frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  password_manager::PasswordGenerationFrameHelper* generation_helper =
      [password_controller_helper_ PasswordGenerationHelper:frame];
  ASSERT_TRUE(generation_helper != nullptr);
}
