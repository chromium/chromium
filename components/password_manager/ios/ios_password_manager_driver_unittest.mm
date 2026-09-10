// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver.h"

#import <string_view>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/test/ios/test_utils.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

using ::autofill::AutofillJavaScriptFeature;
using ::base::SysNSStringToUTF8;
using ::password_manager::PasswordManager;
using ::testing::_;
using ::testing::Return;

namespace {

// URLs used by frames across multiple tests.
constexpr std::string_view kMainFrameUrl = "https://example.com/login";
constexpr std::string_view kMainFrameOriginUrl = "https://example.com/";
constexpr std::string_view kSubframeUrl = "https://subframe.example.com/iframe";
constexpr std::string_view kSubframeOriginUrl = "https://subframe.example.com/";

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const url::Origin&, base::optional_ref<const GURL>),
              (const, override));
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
};

}  // namespace

class IOSPasswordManagerDriverTest : public PlatformTest {
 public:
  IOSPasswordManagerDriverTest() : PlatformTest() {
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world,
                                   std::move(web_frames_manager));

    auto main_frame = web::FakeWebFrame::Create(
        "main-frame",
        /*is_main_frame=*/true, url::Origin::Create(GURL(kMainFrameOriginUrl)));
    main_frame->set_url(GURL(kMainFrameUrl));
    auto sub_frame = web::FakeWebFrame::Create(
        "frame",
        /*is_main_frame=*/false, url::Origin::Create(GURL(kSubframeOriginUrl)));
    sub_frame->set_url(GURL(kSubframeUrl));
    web_state_.SetCurrentURL(GURL(kMainFrameUrl));
    web::WebFrame* main_frame_ptr = main_frame.get();
    web::WebFrame* sub_frame_ptr = sub_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));
    web_frames_manager_->AddWebFrame(std::move(sub_frame));

    password_controller_ = OCMStrictClassMock([SharedPasswordController class]);

    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, &password_manager_);

    main_frame_driver_ =
        IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
            &web_state_, main_frame_ptr);
    subframe_driver_ = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
        &web_state_, sub_frame_ptr);
  }

 protected:
  raw_ptr<web::FakeWebFramesManager, DanglingUntriaged> web_frames_manager_;
  web::FakeWebState web_state_;
  raw_ptr<IOSPasswordManagerDriver> main_frame_driver_;
  raw_ptr<IOSPasswordManagerDriver> subframe_driver_;
  id password_controller_;
  testing::StrictMock<MockPasswordManagerClient> password_manager_client_;
  PasswordManager password_manager_ =
      PasswordManager(&password_manager_client_);
};

// Tests that the drivers have the correct ids.
TEST_F(IOSPasswordManagerDriverTest, GetId) {
  ASSERT_EQ(main_frame_driver_->GetId(), password_manager::DriverId(1));
  ASSERT_EQ(subframe_driver_->GetId(), password_manager::DriverId(2));
}

// Tests the IsInPrimaryMainFrame method.
TEST_F(IOSPasswordManagerDriverTest, IsInPrimaryMainFrame) {
  ASSERT_TRUE(main_frame_driver_->IsInPrimaryMainFrame());
  ASSERT_FALSE(subframe_driver_->IsInPrimaryMainFrame());
}

// Tests the GetLastCommittedURL and GetLastCommittedOrigin methods.
TEST_F(IOSPasswordManagerDriverTest, GetLastCommittedURLAndOrigin) {
  EXPECT_EQ(main_frame_driver_->GetLastCommittedURL(), GURL(kMainFrameUrl));
  EXPECT_EQ(main_frame_driver_->GetLastCommittedOrigin(),
            url::Origin::Create(GURL(kMainFrameOriginUrl)));
  EXPECT_EQ(subframe_driver_->GetLastCommittedURL(), GURL(kSubframeUrl));
  EXPECT_EQ(subframe_driver_->GetLastCommittedOrigin(),
            url::Origin::Create(GURL(kSubframeOriginUrl)));
}

// Tests that GetLastCommittedURL falls back to the origin URL when the frame
// URL is empty and the origin is not opaque.
TEST_F(IOSPasswordManagerDriverTest, GetLastCommittedURLFallbackToOrigin) {
  static constexpr std::string_view kFallbackOriginUrl =
      "https://fallback.example.com/";
  auto empty_url_frame = web::FakeWebFrame::Create(
      "empty-url-frame",
      /*is_main_frame=*/false, url::Origin::Create(GURL(kFallbackOriginUrl)));
  empty_url_frame->set_url(GURL());
  web::WebFrame* frame_ptr = empty_url_frame.get();
  web_frames_manager_->AddWebFrame(std::move(empty_url_frame));

  auto* driver = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
      &web_state_, frame_ptr);
  ASSERT_TRUE(driver);
  EXPECT_EQ(driver->GetLastCommittedURL(), GURL(kFallbackOriginUrl));
}

// Tests that GetLastCommittedURL does not fall back to the origin when the
// frame URL is empty and the origin is opaque.
TEST_F(IOSPasswordManagerDriverTest, GetLastCommittedURLOpaqueOrigin) {
  auto opaque_frame =
      web::FakeWebFrame::Create("opaque-frame",
                                /*is_main_frame=*/false, url::Origin());
  opaque_frame->set_url(GURL());
  web::WebFrame* frame_ptr = opaque_frame.get();
  web_frames_manager_->AddWebFrame(std::move(opaque_frame));

  auto* driver = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
      &web_state_, frame_ptr);
  ASSERT_TRUE(driver);
  EXPECT_EQ(driver->GetLastCommittedURL(), GURL::EmptyGURL());
}

// Tests that GetLastCommittedURL falls back to the cached URL for the main
// frame when WebState's last committed URL is empty.
TEST_F(IOSPasswordManagerDriverTest, GetLastCommittedURLMainFrameFallback) {
  // Set empty URL to the webstate to force fallback to the frame URL.
  web_state_.SetCurrentURL(GURL());
  EXPECT_EQ(main_frame_driver_->GetLastCommittedURL(), GURL(kMainFrameUrl));
}

// Tests that GetLastCommittedURL returns the cached URL when WebState is
// destroyed while the driver outlives it.
TEST_F(IOSPasswordManagerDriverTest, GetLastCommittedURLWhenWebStateDestroyed) {
  static constexpr std::string_view kWebStateUrl =
      "https://example.com/current";
  auto web_state = std::make_unique<web::FakeWebState>();
  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto web_frame = web::FakeWebFrame::Create(
      "main-frame",
      /*is_main_frame=*/true, url::Origin::Create(GURL(kMainFrameOriginUrl)));
  web_frame->set_url(GURL(kMainFrameUrl));
  web::WebFrame* frame_ptr = web_frame.get();
  web_frames_manager->AddWebFrame(std::move(web_frame));
  web::ContentWorld content_world =
      AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
  web_state->SetWebFramesManager(content_world, std::move(web_frames_manager));
  web_state->SetCurrentURL(GURL(kWebStateUrl));

  IOSPasswordManagerDriverFactory::CreateForWebState(
      web_state.get(), password_controller_, &password_manager_);

  scoped_refptr<IOSPasswordManagerDriver> retainable_driver =
      IOSPasswordManagerDriverFactory::GetRetainableDriver(web_state.get(),
                                                           frame_ptr);

  // Returns the WebState URL while it is alive.
  ASSERT_TRUE(retainable_driver);
  EXPECT_EQ(retainable_driver->GetLastCommittedURL(), GURL(kWebStateUrl));

  // Destroy the WebState.
  base::WeakPtr<web::WebState> weak_web_state = web_state->GetWeakPtr();
  ASSERT_TRUE(weak_web_state);
  web_state.reset();
  ASSERT_FALSE(weak_web_state);

  // Verify that the driver returns the WebFrame URL after the WebState is
  // destroyed.
  ASSERT_TRUE(retainable_driver);
  EXPECT_EQ(retainable_driver->GetLastCommittedURL(), GURL(kMainFrameUrl));
}

// Tests the PropagateFillDataOnParsingCompletion method.
TEST_F(IOSPasswordManagerDriverTest, PropagateFillDataOnParsingCompletion) {
  autofill::PasswordFormFillData form_data;

  OCMExpect(
      [[password_controller_ ignoringNonObjectArgs]
          processPasswordFormFillData:form_data
                           forFrameId:""
                          isMainFrame:main_frame_driver_->IsInPrimaryMainFrame()
                    forSecurityOrigin:main_frame_driver_->security_origin()])
      .andCompareObjectAtIndex(main_frame_driver_->web_frame_id(), 1);
  main_frame_driver_->PropagateFillDataOnParsingCompletion(form_data);

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests the InformNoSavedCredentials method.
TEST_F(IOSPasswordManagerDriverTest, InformNoSavedCredentials) {
  const std::string main_frame_id = SysNSStringToUTF8(@"main-frame");
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
                onNoSavedCredentialsWithFrameId:""])
      .andCompareObjectAtIndex(main_frame_id, 0);
  main_frame_driver_->InformNoSavedCredentials(
      /*should_show_popup_without_passwords=*/false);

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests the driver when forms eligible for password generation are found.
// Verifies that the proactive generation can only be used once that it is known
// that there are no passwords for the site.
TEST_F(IOSPasswordManagerDriverTest, FormEligibleForGenerationFound) {
  // Set objects needed to handle 3 forms eligible for password generation.

  // Set store for the client that is able to save passwords.
  auto store =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  EXPECT_CALL(password_manager_client_, GetProfilePasswordStore)
      .WillRepeatedly(testing::Return(store.get()));
  EXPECT_CALL(*store, GetError)
      .Times(3)
      .WillRepeatedly(Return(password_manager::ActionableError::kNoError));

  // Enable password saving and generation in the client.
  EXPECT_CALL(password_manager_client_, IsSavingAndFillingEnabled(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*password_manager_client_.GetPasswordFeatureManager(),
              IsGenerationEnabled())
      .Times(3)
      .WillRepeatedly(Return(true));

  // Set other expected calls on the controller when an eligible form for
  // generation is found.
  autofill::PasswordFormGenerationData form;
  for (int i = 0; i < 3; ++i) {
    OCMExpect([password_controller_ formEligibleForGenerationFound:form]);
  }

  // Inform the driver that 2 forms eligible for generation were found. The
  // driver doesn't know yet at this point whether proactive generation can also
  // be used.
  main_frame_driver_->FormEligibleForGenerationFound(form);
  main_frame_driver_->FormEligibleForGenerationFound(form);

  // Inform that there are no saved credentials for the site so proactive
  // generation is set up on the forms eligible for generation.
  // Verify that the listeners for proactive generation are attached for the
  // 2 forms.
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      attachListenersForPasswordGenerationFields:form
                                      forFrameId:""]);
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      attachListenersForPasswordGenerationFields:form
                                      forFrameId:""]);
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      onNoSavedCredentialsWithFrameId:""]);
  main_frame_driver_->InformNoSavedCredentials(
      /*should_show_popup_without_passwords=*/false);

  // Inform the driver again that an eligible form for generation was found.
  // Verify that the listeners for proactive generation are immediately attached
  // since it is already known that there are no saved credentials for the site.
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      attachListenersForPasswordGenerationFields:form
                                      forFrameId:""]);
  main_frame_driver_->FormEligibleForGenerationFound(form);

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests that the queue of pending forms for proactive generation
// doesn't grow more than its max capacity.
TEST_F(IOSPasswordManagerDriverTest,
       FormEligibleForGenerationFound_MaxProactiveQueueCapacity) {
  // Set objects needed to handle 3 forms eligible for password generation.

  // Set store for the client that is able to save passwords.
  auto store =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  EXPECT_CALL(password_manager_client_, GetProfilePasswordStore)
      .WillRepeatedly(testing::Return(store.get()));
  EXPECT_CALL(*store, GetError)
      .Times(21)
      .WillRepeatedly(Return(password_manager::ActionableError::kNoError));

  // Enable password saving and generation in the client.
  EXPECT_CALL(password_manager_client_, IsSavingAndFillingEnabled(_, _))
      .Times(21)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*password_manager_client_.GetPasswordFeatureManager(),
              IsGenerationEnabled())
      .Times(21)
      .WillRepeatedly(Return(true));

  // Set other expected calls on the controller when an eligible form for
  // generation is found.
  autofill::PasswordFormGenerationData form;
  for (int i = 0; i < 21; ++i) {
    OCMExpect([password_controller_ formEligibleForGenerationFound:form]);
  }

  // Inform the driver that 20 forms eligible for generation were found. The
  // driver doesn't know yet at this point whether proactive generation can also
  // be used.
  for (int i = 0; i < 20; ++i) {
    main_frame_driver_->FormEligibleForGenerationFound(form);
  }

  // Inform that there are no saved credentials for the site so proactive
  // generation is set up on the forms eligible for generation.
  // Verify that the listeners for proactive generation are only attached for
  // the first 10 forms which corresponds to the max capacity of the queue.
  for (int i = 0; i < 10; ++i) {
    OCMExpect([[password_controller_ ignoringNonObjectArgs]
        attachListenersForPasswordGenerationFields:form
                                        forFrameId:""]);
  }
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      onNoSavedCredentialsWithFrameId:""]);
  main_frame_driver_->InformNoSavedCredentials(
      /*should_show_popup_without_passwords=*/false);

  // Inform the driver again that an eligible form for generation was found.
  // Since the queue is now cleared, verify that the listeners for proactive
  // generation are immediately attached since it is already known that there
  // are no saved credentials for the site.
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      attachListenersForPasswordGenerationFields:form
                                      forFrameId:""]);

  main_frame_driver_->FormEligibleForGenerationFound(form);

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests that FillField on the driver correctly forwards to the bridge.
TEST_F(IOSPasswordManagerDriverTest, FillField) {
  autofill::FieldRendererId field_id(42);
  std::u16string value = u"test_password";
  std::string frame_id = main_frame_driver_->web_frame_id();

  OCMExpect([password_controller_ fillField:field_id
                                  withValue:value
                                 forFrameId:frame_id
                          completionHandler:[OCMArg any]])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation* invocation) {
        std::remove_reference_t<decltype(field_id)> param;
        [invocation getArgument:&param atIndex:2];
        EXPECT_EQ(param, field_id);
      })
      .andCompareObjectAtIndex(value, 1)
      .andCompareObjectAtIndex(frame_id, 2);

  main_frame_driver_->FillField(field_id, value,
                                autofill::FieldPropertiesFlags::kNoFlags,
                                base::DoNothing());

  EXPECT_OCMOCK_VERIFY(password_controller_);
}

// Tests that CheckViewAreaVisible on the driver correctly forwards to the
// bridge.
TEST_F(IOSPasswordManagerDriverTest, CheckViewAreaVisible) {
  autofill::FieldRendererId field_id(42);
  std::string frame_id = main_frame_driver_->web_frame_id();

  OCMExpect([password_controller_ scrollAndCheckViewAreaVisible:field_id
                                                     forFrameId:frame_id
                                              completionHandler:[OCMArg any]])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation* invocation) {
        std::remove_reference_t<decltype(field_id)> param;
        [invocation getArgument:&param atIndex:2];
        EXPECT_EQ(param, field_id);
      })
      .andCompareObjectAtIndex(frame_id, 1);

  main_frame_driver_->CheckViewAreaVisible(field_id, base::DoNothing());

  EXPECT_OCMOCK_VERIFY(password_controller_);
}
