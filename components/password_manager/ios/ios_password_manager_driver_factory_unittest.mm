// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver_factory.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class IOSPasswordManagerDriverFactoryTest : public PlatformTest {
 public:
  IOSPasswordManagerDriverFactoryTest() : PlatformTest() {
    password_manager_ = std::make_unique<password_manager::PasswordManager>(
        &password_manager_client_);
    password_controller_ = OCMStrictClassMock([SharedPasswordController class]);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));
  }

 protected:
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  std::unique_ptr<password_manager::PasswordManager> password_manager_;
  SharedPasswordController* password_controller_;
  password_manager::StubPasswordManagerClient password_manager_client_;
};

// Tests the complete flow of driver creation: factory creation and retrieval,
// driver creation and retrieval and retainable driver creation.
TEST_F(IOSPasswordManagerDriverFactoryTest, CreateFactoryAndDriver) {
  IOSPasswordManagerDriverFactory::CreateForWebState(
      &web_state_, password_controller_, password_manager_.get());

  IOSPasswordManagerDriverFactory* factory =
      IOSPasswordManagerDriverFactory::FromWebState(&web_state_);
  ASSERT_TRUE(factory != nullptr);

  auto web_frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  web::FakeWebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  // driver_created and driver_retrieved should point to the same driver.
  // The first call to IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame
  // creates the driver, whilst the second one retrieves it.
  IOSPasswordManagerDriver* driver_created =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(&web_state_,
                                                               frame);
  IOSPasswordManagerDriver* driver_retrieved =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(&web_state_,
                                                               frame);
  IOSPasswordManagerDriver* no_driver_created =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(&web_state_,
                                                               nullptr);

  ASSERT_TRUE(driver_created != nullptr);
  ASSERT_EQ(driver_created, driver_retrieved);
  ASSERT_TRUE(no_driver_created == nullptr);

  // If everything worked, the driver should exist and have an id equal to 0.
  ASSERT_EQ(driver_created->GetId(), 0);

  auto retainable_driver =
      IOSPasswordManagerDriverFactory::GetRetainableDriver(&web_state_, frame);
  ASSERT_TRUE(retainable_driver != nullptr);
  // The retainable version of the driver should exist.
  ASSERT_EQ(retainable_driver.get(), driver_created);
}

// Tests that a driver is created inside GetRetainableDriver method if the
// driver didn't exits before.
TEST_F(IOSPasswordManagerDriverFactoryTest,
       CreateDriverFromGetRetainableDriver) {
  IOSPasswordManagerDriverFactory::CreateForWebState(
      &web_state_, password_controller_, password_manager_.get());

  auto web_frame = web::FakeWebFrame::CreateMainWebFrame(GURL());
  web::FakeWebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  auto retainable_driver =
      IOSPasswordManagerDriverFactory::GetRetainableDriver(&web_state_, frame);
  ASSERT_TRUE(retainable_driver != nullptr);

  // If everything worked, the driver should exist and have an id equal to 0.
  ASSERT_EQ(retainable_driver.get()->GetId(), 0);
}
