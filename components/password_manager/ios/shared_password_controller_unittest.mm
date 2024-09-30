// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/shared_password_controller.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/autofill_driver_router.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/browser/test_browser_autofill_manager.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_data_test_api.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/core/common/password_generation_util.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#import "components/autofill/ios/browser/password_autofill_agent.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/mock_password_manager.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager_interface.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/core/browser/stub_password_manager_driver.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/constants.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "components/password_manager/ios/shared_password_controller+private.h"
#import "components/password_manager/ios/test_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#define andCompareStringAtIndex(expected_string, index) \
  andDo(^(NSInvocation * invocation) {                  \
    const std::string* param;                           \
    [invocation getArgument:&param atIndex:index + 2];  \
    EXPECT_EQ(*param, expected_string);                 \
  })

namespace password_manager {

namespace {

using autofill::AutofillDriverIOS;
using autofill::AutofillDriverIOSFactory;
using autofill::FormData;
using autofill::LocalFrameToken;
using autofill::PasswordFormFillData;
using autofill::RemoteFrameToken;
using autofill::TestAutofillManagerInjector;
using autofill::TestBrowserAutofillManager;
using autofill::password_generation::PasswordGenerationType;
using autofill::test::CreateTestFormField;
using autofill::test::MakeLocalFrameToken;
using autofill::test::MakeRemoteFrameToken;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using password_manager::IsCrossOriginIframe;
using password_manager::PasswordGenerationFrameHelper;
using ::testing::_;
using ::testing::Return;

const std::string kTestURL = "https://www.chromium.org/";
NSString* kTestFrameID = @"dummy-frame-id";
constexpr uint64_t kMaxPasswordLength = 10;
constexpr char16_t kGeneratedPassword[] = u"testpassword";

// Creates renderer FormData tied to a frame that can compose a xframe browser
// form.
FormData CreateFormDataForRenderFrameHost(
    web::WebFrame* frame,
    LocalFrameToken host_frame_token,
    std::vector<autofill::FormFieldData> fields) {
  FormData form;
  form.set_url(frame->GetSecurityOrigin());
  form.set_action(form.url());
  form.set_host_frame(host_frame_token);
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  for (autofill::FormFieldData& field : fields) {
    field.set_host_frame(form.host_frame());
    field.set_host_form_id(form.renderer_id());
  }
  form.set_fields(std::move(fields));
  return form;
}

// Creates a basic signup form with one username field and one password field.
FormData CreateSignupForm() {
  FormData form_data = test_helpers::MakeSimpleFormData();
  form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  for (auto& field_data : test_api(form_data).fields()) {
    field_data.set_renderer_id(autofill::test::MakeFieldRendererId());
  }
  test_api(form_data).fields().back().set_max_length(kMaxPasswordLength);
  return form_data;
}

}  // namespace

class MockPasswordGenerationFrameHelper : public PasswordGenerationFrameHelper {
 public:
  MOCK_METHOD(std::u16string,
              GeneratePassword,
              (const GURL&,
               autofill::password_generation::PasswordGenerationType,
               autofill::FormSignature,
               autofill::FieldSignature,
               uint64_t),
              (override));

  MOCK_METHOD(bool, IsGenerationEnabled, (bool), (const));

  explicit MockPasswordGenerationFrameHelper()
      : PasswordGenerationFrameHelper(nullptr, nullptr) {}
};

class SharedPasswordControllerTest : public PlatformTest {
 public:
  SharedPasswordControllerTest() : PlatformTest() {
    delegate_ = OCMProtocolMock(@protocol(SharedPasswordControllerDelegate));
    password_manager::PasswordManagerClient* client_ptr =
        &password_manager_client_;
    [[[delegate_ stub] andReturnValue:OCMOCK_VALUE(client_ptr)]
        passwordManagerClient];
    form_helper_ = OCMStrictClassMock([PasswordFormHelper class]);
    suggestion_helper_ = OCMStrictClassMock([PasswordSuggestionHelper class]);
    driver_helper_ = OCMStrictClassMock([PasswordControllerDriverHelper class]);

    OCMExpect([form_helper_ setDelegate:[OCMArg any]]);
    OCMExpect([suggestion_helper_ setDelegate:[OCMArg any]]);

    // Some tests observe both frame managers. Making sure that both content
    // world have managers set.
    web_state_.SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        PasswordManagerJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world,
                                   std::move(web_frames_manager));

    AutofillDriverIOSFactory::CreateForWebState(
        &web_state_, &autofill_client_, /*bridge=*/nil, /*locale=*/"en");
    // The manager injector must be created before creating the controller to
    // make sure it can exchange the manager before the controller starts
    // observing it.
    autofill_manager_injector_ = std::make_unique<
        TestAutofillManagerInjector<TestBrowserAutofillManager>>(&web_state_);

    controller_ =
        [[SharedPasswordController alloc] initWithWebState:&web_state_
                                                   manager:&password_manager_
                                                formHelper:form_helper_
                                          suggestionHelper:suggestion_helper_
                                              driverHelper:driver_helper_];
    controller_.delegate = delegate_;
    [suggestion_helper_ verify];
    [form_helper_ verify];

    web_state_.SetCurrentURL(GURL(kTestURL));
  }

  void SetUp() override {
    PlatformTest::SetUp();

    PasswordGenerationFrameHelper* password_generation_helper_ptr =
        &password_generation_helper_;
    OCMStub(
        [driver_helper_ PasswordGenerationHelper:static_cast<web::WebFrame*>(
                                                     [OCMArg anyPointer])])
        .andReturn(password_generation_helper_ptr);

    EXPECT_CALL(password_manager_, GetClient)
        .WillRepeatedly(Return(&password_manager_client_));

    // It's not possible to mock the driver, and it has to be non-null, so we
    // have the mock DriverHelper return a real driver.
    auto dummy_web_frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL(kTestURL));
    dummy_driver_ = IOSPasswordManagerDriverFactory::GetRetainableDriver(
        &web_state_, dummy_web_frame.get());
    OCMStub([driver_helper_ PasswordManagerDriver:static_cast<web::WebFrame*>(
                                                      [OCMArg anyPointer])])
        .andReturn(dummy_driver_.get());
  }

 protected:
  void AddWebFrame(std::unique_ptr<web::WebFrame> frame,
                   id completion_handler) {
    OCMExpect([form_helper_ findPasswordFormsInFrame:frame.get()
                                   completionHandler:completion_handler]);

    web_frames_manager_->AddWebFrame(std::move(frame));
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    AddWebFrame(std::move(frame), [OCMArg any]);
  }

  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillManagerInjector<TestBrowserAutofillManager>>
      autofill_manager_injector_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  testing::StrictMock<MockPasswordManager> password_manager_;
  testing::StrictMock<MockPasswordGenerationFrameHelper>
      password_generation_helper_;
  id form_helper_;
  id suggestion_helper_;
  id driver_helper_;
  scoped_refptr<IOSPasswordManagerDriver> dummy_driver_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  id delegate_;
  SharedPasswordController* controller_;
};

TEST_F(SharedPasswordControllerTest,
       PasswordManagerIsNotNotifiedAboutHeuristicsPredictions) {
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, ProcessAutofillPredictions).Times(0);

  // Simulate seeing a form.
  TestBrowserAutofillManager* manager =
      autofill_manager_injector_->GetForFrame(frame);
  ASSERT_TRUE(manager);
  FormData test_form = autofill::test::CreateTestPersonalInformationFormData();
  // `OnFormsSeen` emits a `OnFieldTypesDetermined` event, but with source
  // heuristics - this should be ignored by the `SharedPasswordController`.
  manager->OnFormsSeen(/*updated_forms=*/{test_form}, /*removed_forms=*/{});
}

// Tests that the password manager of a single main frame is notified about
// server predictions for a form.
TEST_F(SharedPasswordControllerTest,
       PasswordManagerIsNotifiedAboutServerPredictions_SingleFrame) {
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, ProcessAutofillPredictions);

  // Simulate seeing a form.
  TestBrowserAutofillManager* manager =
      autofill_manager_injector_->GetForFrame(frame);
  ASSERT_TRUE(manager);
  FormData test_form = autofill::test::CreateTestPersonalInformationFormData();
  manager->OnFormsSeen(/*updated_forms=*/{test_form}, /*removed_forms=*/{});

  // Trigger `OnFieldTypesDetetermined` with source `kAutofillServer` explicitly
  // to simulate receiving server predictions.
  using Observer = autofill::AutofillManager::Observer;
  manager->NotifyObservers(&Observer::OnFieldTypesDetermined,
                           test_form.global_id(),
                           Observer::FieldTypeSource::kAutofillServer);
}

// Tests that the password manager of each frame is notified about server
// predictions when a form streches across multiple frames.
TEST_F(SharedPasswordControllerTest,
       PasswordManagerIsNotifiedAboutServerPredictions_AcrossFrames) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAcrossIframesIos);

  const LocalFrameToken main_frame_local_frame_token(MakeLocalFrameToken());
  const LocalFrameToken child_frame_local_token(MakeLocalFrameToken());
  const RemoteFrameToken child_frame_remote_token(MakeRemoteFrameToken());

  auto main_frame =
      web::FakeWebFrame::Create(main_frame_local_frame_token.ToString(),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* main_frame_ptr = main_frame.get();
  AddWebFrame(std::move(main_frame));

  auto child_frame =
      web::FakeWebFrame::Create(child_frame_local_token.ToString(),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* child_frame_ptr = child_frame.get();
  AddWebFrame(std::move(child_frame));

  FormData main_form = CreateFormDataForRenderFrameHost(
      main_frame_ptr, main_frame_local_frame_token,
      {CreateTestFormField("Search", "search", "",
                           autofill::FormControlType::kInputText)});
  {
    autofill::FrameTokenWithPredecessor child_token;
    child_token.token = child_frame_remote_token;
    child_token.predecessor = 64;
    main_form.set_child_frames({child_token});
  }

  FormData child_form = CreateFormDataForRenderFrameHost(
      child_frame_ptr, child_frame_local_token,
      {CreateTestFormField("Username", "username", "",
                           autofill::FormControlType::kInputText),
       CreateTestFormField("Password", "password", "",
                           autofill::FormControlType::kInputPassword)});

  // Simulate seeing a form in the main frame.
  {
    auto* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_, main_frame_ptr);
    driver->FormsSeen(/*updated_forms=*/{main_form},
                      /*removed_forms=*/{});
  }

  // Simulate seeing a form in the child frame.
  {
    auto* driver = AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_,
                                                              child_frame_ptr);
    driver->FormsSeen(/*updated_forms=*/{child_form},
                      /*removed_forms=*/{});
  }

  // Complete registration of the child frame so it can be included in the
  // browser form.
  {
    auto* registrar =
        autofill::ChildFrameRegistrar::GetOrCreateForWebState(&web_state_);
    registrar->RegisterMapping(child_frame_remote_token,
                               child_frame_local_token);
  }

  // Verify that the browser form is correctly constructed, composed of the main
  // frame form and child frame form, having 3 fields in total.
  autofill::FormStructure* browser_form =
      autofill_manager_injector_->GetForFrame(main_frame_ptr)
          ->FindCachedFormById(main_form.global_id());
  ASSERT_TRUE(browser_form);
  ASSERT_EQ(3u, browser_form->field_count());

  // Expect to process predictions for each renderer form that composes the
  // browser form. Validate that the arguments are correct.
  {
    auto* password_driver =
        IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
            &web_state_, main_frame_ptr);
    EXPECT_CALL(
        password_manager_,
        ProcessAutofillPredictions(password_driver,
                                   ::testing::Property(&FormData::renderer_id,
                                                       main_form.renderer_id()),
                                   ::testing::SizeIs(3)));
  }
  {
    auto* password_driver =
        IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
            &web_state_, child_frame_ptr);
    EXPECT_CALL(password_manager_,
                ProcessAutofillPredictions(
                    password_driver,
                    ::testing::Property(&FormData::renderer_id,
                                        child_form.renderer_id()),
                    ::testing::SizeIs(3)));
  }

  // Trigger `OnFieldTypesDetetermined` with source `kAutofillServer` explicitly
  // to simulate receiving server predictions.
  // using Observer = autofill::AutofillManager::Observer;
  TestBrowserAutofillManager* manager =
      autofill_manager_injector_->GetForFrame(main_frame_ptr);
  using Observer = autofill::AutofillManager::Observer;
  manager->NotifyObservers(&Observer::OnFieldTypesDetermined,
                           main_form.global_id(),
                           Observer::FieldTypeSource::kAutofillServer);
}

// Test that PasswordManager is notified of main frame navigation.
TEST_F(SharedPasswordControllerTest,
       PasswordManagerDidNavigationMainFrameOnNavigationFinished) {
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  navigation_context.SetIsSameDocument(false);
  navigation_context.SetIsRendererInitiated(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, DidNavigateMainFrame(true));
  OCMExpect([suggestion_helper_ resetForNewPage]);
  web_state_.OnNavigationFinished(&navigation_context);
}

// Tests that forms are found, parsed, and sent to PasswordManager.
TEST_F(SharedPasswordControllerTest, FormsArePropagatedOnHTMLPageLoad) {
  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create("dummy-frame-id",
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  id mock_completion_handler = [OCMArg checkWithBlock:^(void (
      ^completionHandler)(const std::vector<FormData>& forms, uint32_t maxID)) {
    OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
    EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
    EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
    FormData form_data = test_helpers::MakeSimpleFormData();
    completionHandler({form_data}, 1);
    return YES;
  }];
  AddWebFrame(std::move(web_frame), mock_completion_handler);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests form finding and parsing is not triggered for non HTML pages.
TEST_F(SharedPasswordControllerTest, NoFormsArePropagatedOnNonHTMLPageLoad) {
  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(false);

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame = web::FakeWebFrame::Create(
      web_frame_id, /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [[form_helper_ reject] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];
  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
                processWithNoSavedCredentialsWithFrameId:""])
      .andCompareStringAtIndex(web_frame_id, 0);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests that suggestions are reported as unavailable for nonpassword forms.
TEST_F(SharedPasswordControllerTest,
       CheckNoSuggestionsAreAvailableForNonPasswordForm) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  id mock_completion_handler =
      [OCMArg checkWithBlock:^BOOL(void (^completionHandler)(BOOL)) {
        // Ensure |suggestion_helper_| reports no suggestions.
        completionHandler(NO);
        return YES;
      }];
  [[suggestion_helper_ expect]
      checkIfSuggestionsAvailableForForm:form_query
                       completionHandler:mock_completion_handler];
  [[suggestion_helper_ expect] isPasswordFieldOnForm:form_query webFrame:frame];

  __block BOOL completion_was_called = NO;
  [controller_ checkIfSuggestionsAvailableForForm:form_query
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  EXPECT_FALSE(suggestionsAvailable);
                                  completion_was_called = YES;
                                }];
  EXPECT_TRUE(completion_was_called);

  [suggestion_helper_ verify];
}

// Tests that no suggestions are returned if PasswordSuggestionHelper has none.
TEST_F(SharedPasswordControllerTest, ReturnsNoSuggestionsIfNoneAreAvailable) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:kObfuscatedFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];
  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(@[]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
      isPasswordFieldOnForm:form_query
                   webFrame:frame]);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, IsGenerationEnabled(true))
      .WillOnce(Return(true));

  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 EXPECT_EQ(0UL, suggestions.count);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that no suggestions are returned if the frame was destroyed.
TEST_F(SharedPasswordControllerTest, ReturnsNoSuggestionsIfFrameDestroyed) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:kObfuscatedFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  web::WebFrame* frame = nullptr;
  const std::string frame_id = "";

  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(@[]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
      isPasswordFieldOnForm:form_query
                   webFrame:frame]);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 EXPECT_EQ(0UL, suggestions.count);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that suggestions are returned if PasswordSuggestionHelper has some.
TEST_F(SharedPasswordControllerTest, ReturnsSuggestionsIfAvailable) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:kObfuscatedFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];
  FormSuggestion* suggestion = [FormSuggestion
             suggestionWithValue:@"value"
              displayDescription:@"display-description"
                            icon:nil
                            type:autofill::SuggestionType::kAutocompleteEntry
               backendIdentifier:nil
                  requiresReauth:NO
      acceptanceA11yAnnouncement:nil
                        metadata:{.is_single_username_form = true}];

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(@[ suggestion ]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
      isPasswordFieldOnForm:form_query
                   webFrame:frame]);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, IsGenerationEnabled(true))
      .WillOnce(Return(true));

  __block BOOL completion_was_called = NO;
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 EXPECT_EQ(1UL, suggestions.count);
                 // Verify that the metadata is correctly copied over.
                 EXPECT_TRUE([suggestions firstObject]
                                 .metadata.is_single_username_form);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that the "Suggest a password" suggestion is returned if the form is
// eligible for generation.
TEST_F(SharedPasswordControllerTest,
       ReturnsGenerateSuggestionIfFormIsEligibleForGeneration) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:kObfuscatedFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(@[]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
      isPasswordFieldOnForm:form_query
                   webFrame:frame]);

  autofill::PasswordFormGenerationData form_generation_data = {
      form_query.formRendererID, form_query.fieldRendererID,
      form_query.fieldRendererID};
  [controller_ formEligibleForGenerationFound:form_generation_data];
  __block BOOL completion_was_called = NO;

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, IsGenerationEnabled(true))
      .WillOnce(Return(true));
  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 ASSERT_EQ(1UL, suggestions.count);
                 FormSuggestion* suggestion = suggestions.firstObject;
                 EXPECT_EQ(autofill::SuggestionType::kGeneratePasswordEntry,
                           suggestion.type);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that accepting a "Suggest a password" suggestion will give a suggested
// password to the delegate.
TEST_F(SharedPasswordControllerTest, SuggestsGeneratedPassword) {
  FormData form_data = CreateSignupForm();

  const autofill::FormRendererId form_id = form_data.renderer_id();
  autofill::FormFieldData password_field_data = form_data.fields().back();
  const autofill::FieldRendererId field_id = password_field_data.renderer_id();

  // Register the form as eligible for password generation while specifying that
  // the form has no confirmation password field. This is to allow generating a
  // password on that form.
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, field_id,
      /*confirmation_field=*/autofill::FieldRendererId(0)};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  OCMExpect([delegate_
            sharedPasswordController:controller_
      showGeneratedPotentialPassword:SysUTF16ToNSString(kGeneratedPassword)
                           proactive:NO
                     decisionHandler:[OCMArg isNotNil]]);
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  // Set up the frame that hosts the new password form.
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  // Emulate completing the extraction.
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  OCMExpect([form_helper_
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg]);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  // Mock generating a valid password and verify that GeneratePassword() is
  // correctly called.
  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureForField(password_field_data);
  EXPECT_CALL(
      password_generation_helper_,
      GeneratePassword(web_state_.GetLastCommittedURL(),
                       PasswordGenerationType::kAutomatic, form_signature,
                       field_signature, kMaxPasswordLength))
      .WillOnce(::testing::Return(kGeneratedPassword));

  // Select suggestion to start the password generation flow which will stop at
  // showing the generated pasword.
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
                     type:autofill::SuggestionType::kGeneratePasswordEntry
        backendIdentifier:nil
           requiresReauth:NO];
  [controller_ didSelectSuggestion:suggestion
                           atIndex:0
                              form:@"test-form-name"
                    formRendererID:form_id
                   fieldIdentifier:@"test-field-id"
                   fieldRendererID:field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that generated passwords are presaved.
TEST_F(SharedPasswordControllerTest, PresavesGeneratedPassword) {
  base::HistogramTester histogram_tester;

  FormData form_data = CreateSignupForm();
  autofill::FormRendererId form_id = form_data.renderer_id();
  autofill::FieldRendererId password_field_id =
      form_data.fields()[1].renderer_id();

  // Register the form as eligible for password generation while specifying that
  // the form has no confirmation password field. This is to allow generating a
  // password on that form.
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, password_field_id, password_field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(true);

  // Set up the frame that hosts the new password form.
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  // Show and accept password in the same loop.
  id decision_handler_arg =
      [OCMArg checkWithBlock:^(void (^decision_handler)(BOOL)) {
        decision_handler(/*accept=*/YES);
        return YES;
      }];
  OCMExpect([delegate_ sharedPasswordController:controller_
                 showGeneratedPotentialPassword:[OCMArg isNotNil]
                                      proactive:NO
                                decisionHandler:decision_handler_arg]);

  // Emulating completing filling of the generated password in the renderer.
  id fill_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL)) {
        completion_handler(/*success=*/YES);
        return YES;
      }];
  OCMExpect([form_helper_
               fillPasswordForm:form_id
                        inFrame:frame
          newPasswordIdentifier:password_field_id
      confirmPasswordIdentifier:password_field_id
              generatedPassword:SysUTF16ToNSString(kGeneratedPassword)
              completionHandler:fill_completion_handler_arg]);

  // Complete form extraction before generating the password.
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  OCMExpect([form_helper_
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg]);
  OCMStub([driver_helper_ PasswordManagerDriver:frame]);

  // Mock generating a valid password.
  EXPECT_CALL(password_generation_helper_, GeneratePassword)
      .WillOnce(::testing::Return(kGeneratedPassword));

  EXPECT_CALL(password_manager_, OnPresaveGeneratedPassword);

  // Start the password generation flow by selecting the corresponding
  // suggestion which will end up presaving the generated password.
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
                     type:autofill::SuggestionType::kGeneratePasswordEntry
        backendIdentifier:nil
           requiresReauth:NO];
  [controller_ didSelectSuggestion:suggestion
                           atIndex:0
                              form:@"test-form-name"
                    formRendererID:form_id
                   fieldIdentifier:@"test-field-id"
                   fieldRendererID:password_field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  EXPECT_OCMOCK_VERIFY(delegate_);

  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.AcceptedGeneratedPasswordSource",
      AcceptedGeneratedPasswordSourceType::kSuggestion, 1);
  // Verify that the non-empty generated password occurence is recorded.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.GeneratedPasswordIsEmpty.BeforeParsing", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.AcceptedGeneratedPasswordIsEmpty", 0, 1);
  // No metric for the bottom sheet should be recorded since an another
  // entrypoint was used.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.TouchToFill.PasswordGeneration.UserChoice", 0);
}

// Tests that generated passwords that are empty aren't presaved.
TEST_F(SharedPasswordControllerTest, PresavesGeneratedPassword_Empty) {
  base::HistogramTester histogram_tester;

  FormData form_data = CreateSignupForm();
  const autofill::FormRendererId form_id = form_data.renderer_id();
  const autofill::FieldRendererId password_field_id =
      form_data.fields()[1].renderer_id();

  // Register the form as eligible for password generation to allow password
  // generation on that form.
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, password_field_id, password_field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(true);

  // Set up the frame that hosts the sign up form.
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  // Show and accept password in the same loop.
  id decision_handler_arg =
      [OCMArg checkWithBlock:^(void (^decision_handler)(BOOL)) {
        decision_handler(/*accept=*/YES);
        return YES;
      }];
  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                                     proactive:NO
                               decisionHandler:decision_handler_arg];

  // Emulate completing the extraction before generating the password.
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg];
  OCMStub([driver_helper_ PasswordManagerDriver:frame]);

  // Emulate generating an empty password.
  EXPECT_CALL(password_generation_helper_, GeneratePassword)
      .WillOnce(::testing::Return(u""));

  // Verify that presaving is never done because the generated password is empty
  // hence invalid for presaving. The
  EXPECT_CALL(password_manager_, OnPresaveGeneratedPassword).Times(0);
  EXPECT_CALL(password_manager_, OnPasswordNoLongerGenerated);

  // Start the password generation flow by selecting the corresponding
  // suggestion which will be interrupted at the presaving stage because the
  // generated password is invalid.
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
                     type:autofill::SuggestionType::kGeneratePasswordEntry
        backendIdentifier:nil
           requiresReauth:NO];
  [controller_ didSelectSuggestion:suggestion
                           atIndex:0
                              form:@"test-form-name"
                    formRendererID:form_id
                   fieldIdentifier:@"test-field-id"
                   fieldRendererID:password_field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  EXPECT_OCMOCK_VERIFY(delegate_);

  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_ACCEPTED, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.AcceptedGeneratedPasswordSource",
      AcceptedGeneratedPasswordSourceType::kSuggestion, 1);

  // Verify that the empty generated password occurence is recorded at each
  // stage of the flow.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.GeneratedPasswordIsEmpty.BeforeParsing", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.AcceptedGeneratedPasswordIsEmpty", 1, 1);
  // No metric for the bottom sheet should be recorded since an another
  // entrypoint was used.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.TouchToFill.PasswordGeneration.UserChoice", 0);
}

// Tests that triggering password generation on the last focused field triggers
// the generation flow.
TEST_F(SharedPasswordControllerTest, TriggerPasswordGeneration) {
  base::HistogramTester histogram_tester;
  autofill::FormActivityParams params;
  params.form_renderer_id = autofill::FormRendererId(0);
  params.field_type = "password";
  params.field_renderer_id = autofill::FieldRendererId(1);
  params.type = "focus";
  params.input_missing = false;

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:frame];

  OCMExpect([delegate_ sharedPasswordController:controller_
                 showGeneratedPotentialPassword:[OCMArg isNotNil]
                                      proactive:NO
                                decisionHandler:[OCMArg any]]);
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  FormData form_data = test_helpers::MakeSimpleFormData();
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  OCMExpect([form_helper_
      extractPasswordFormData:params.form_renderer_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg]);
  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, GeneratePassword)
      .WillOnce(Return(u"testpass"));

  [controller_ triggerPasswordGeneration];

  EXPECT_OCMOCK_VERIFY(delegate_);

  // Verify that the metrics that verify the emptyness of the generated password
  // are recorded.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.GeneratedPasswordIsEmpty.BeforeParsing", 0, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED,
      1);
}

// Tests that triggering password generation proactively (aka the proactive
// password generation bottom sheet).
TEST_F(SharedPasswordControllerTest, TriggerPasswordGeneration_Proactively) {
  base::HistogramTester histogram_tester;

  FormData form_data = CreateSignupForm();
  const autofill::FormRendererId form_id = form_data.renderer_id();
  const autofill::FieldRendererId password_field_id =
      form_data.fields()[1].renderer_id();

  // Register the form as eligible for password generation to allow password
  // generation on that form.
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, password_field_id, password_field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  // Set up the frame that hosts the sign up form.
  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  // Verify that the potential password is shown as the proactive variant.
  OCMExpect([delegate_ sharedPasswordController:controller_
                 showGeneratedPotentialPassword:[OCMArg isNotNil]
                                      proactive:YES
                                decisionHandler:[OCMArg any]]);

  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  // Emulate completing form extraction before generating the password.
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  OCMExpect([form_helper_
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg]);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  // Emulate generating a non-empty password.
  EXPECT_CALL(password_generation_helper_, GeneratePassword)
      .WillOnce(Return(u"testpass"));

  // Trigger password generation proactively to start the password generation
  // flow which will stop at the stage where it is shown to the user through the
  // bottom sheet.
  [controller_ triggerPasswordGenerationForFormId:form_id
                                  fieldIdentifier:password_field_id
                                          inFrame:frame
                                        proactive:YES];

  EXPECT_OCMOCK_VERIFY(delegate_);

  // Verify that the metrics that verify the emptyness of the generated password
  // are recorded.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.iOS.GeneratedPasswordIsEmpty.BeforeParsing", 0, 1);
  // Verify that the proactive trigger isn't wrongly recorded as the manual
  // fallback trigger.
  histogram_tester.ExpectBucketCount(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED,
      0);
}

// Tests that triggering password generation on the last focused field does not
// trigger the generation flow if the last reported form activity did not
// provide valid form and field identifiers.
TEST_F(SharedPasswordControllerTest, LastFocusedFieldData) {
  autofill::FormActivityParams params;
  params.form_renderer_id = autofill::FormRendererId(0);
  params.field_type = "password";
  params.field_renderer_id = autofill::FieldRendererId(1);
  params.type = "focus";
  params.input_missing = true;

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:web_frame.get()];

  [[delegate_ reject] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                                     proactive:NO
                               decisionHandler:[OCMArg any]];

  [controller_ triggerPasswordGeneration];

  [delegate_ verify];
}

// Tests that detecting element additions (form_changed events) is not
// interpreted as form submissions.
TEST_F(SharedPasswordControllerTest,
       DoesntInterpretElementAdditionsAsFormSubmission) {
  id mock_completion_handler = [OCMArg checkWithBlock:^(void (
      ^completionHandler)(const std::vector<FormData>& forms, uint32_t maxID)) {
    EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
    // OnPasswordFormsRendered is responsible for detecting submissions.
    // Making sure it's not called after element additions.
    EXPECT_CALL(password_manager_, OnPasswordFormsRendered).Times(0);
    FormData form_data = test_helpers::MakeSimpleFormData();
    completionHandler({form_data}, 1);
    return YES;
  }];

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  AddWebFrame(std::move(web_frame));

  OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                 completionHandler:mock_completion_handler]);

  autofill::FormActivityParams params;
  params.type = "form_changed";
  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:frame];

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

class SharedPasswordControllerTestWithRealSuggestionHelper
    : public PlatformTest {
 public:
  SharedPasswordControllerTestWithRealSuggestionHelper() : PlatformTest() {
    delegate_ = OCMProtocolMock(@protocol(SharedPasswordControllerDelegate));
    password_manager::PasswordManagerClient* client_ptr =
        &password_manager_client_;
    [[[delegate_ stub] andReturnValue:OCMOCK_VALUE(client_ptr)]
        passwordManagerClient];

    suggestion_helper_ =
        [[PasswordSuggestionHelper alloc] initWithWebState:&web_state_];

    form_helper_ = OCMStrictClassMock([PasswordFormHelper class]);
    OCMExpect([form_helper_ setDelegate:[OCMArg any]]);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        PasswordManagerJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world,
                                   std::move(web_frames_manager));

    PasswordControllerDriverHelper* driver_helper =
        [[PasswordControllerDriverHelper alloc] initWithWebState:&web_state_];
    controller_ =
        [[SharedPasswordController alloc] initWithWebState:&web_state_
                                                   manager:&password_manager_
                                                formHelper:form_helper_
                                          suggestionHelper:suggestion_helper_
                                              driverHelper:driver_helper];
    [form_helper_ verify];

    controller_.delegate = delegate_;

    AutofillDriverIOSFactory::CreateForWebState(
        &web_state_, &autofill_client_, /*bridge=*/nil, /*locale=*/"en");

    web_state_.SetCurrentURL(GURL(kTestURL));
  }

  void SetUp() override {
    PlatformTest::SetUp();

    EXPECT_CALL(password_manager_, GetClient)
        .WillRepeatedly(Return(&password_manager_client_));
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    web_frames_manager_->AddWebFrame(std::move(frame));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  autofill::TestAutofillClient autofill_client_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  testing::StrictMock<MockPasswordManager> password_manager_;
  PasswordSuggestionHelper* suggestion_helper_;
  id form_helper_;
  id delegate_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  SharedPasswordController* controller_;
};

// Tests the completion handler for suggestions availability is not called
// until password manager replies with suggestions.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       WaitForPasswordmanagerResponseToShowSuggestions) {
  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);

  [controller_ didFinishPasswordFormExtraction:{form}
                         triggeredByFormChange:false
                                       inFrame:frame];

  // Simulate user focusing the field in a form before the password store
  // response is received.
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:SysUTF16ToNSString(form.name())
        formRendererID:form.renderer_id()
       fieldIdentifier:SysUTF16ToNSString(form.fields()[0].name())
       fieldRendererID:form.fields()[0].renderer_id()
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  __block BOOL completion_was_called = NO;

  [controller_ checkIfSuggestionsAvailableForForm:form_query
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  completion_was_called = YES;
                                }];

  // Check that completion handler wasn't called.
  EXPECT_FALSE(completion_was_called);

  // Receive suggestions from PasswordManager.
  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      form.url().spec(), "", form.renderer_id().value(), "",
      form.fields()[0].renderer_id().value(), "john.doe@gmail.com", "",
      form.fields()[1].renderer_id().value(), "super!secret", nullptr, nullptr,
      &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                forFrameId:web_frame_id
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  // Check that completion handler was called.
  EXPECT_TRUE(completion_was_called);
}

// Tests the completion handler for suggestions availability is not called
// until password manager replies with suggestions.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       WaitForPasswordManagerResponseToShowSuggestionsTwoFields) {
  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);

  [controller_ didFinishPasswordFormExtraction:{form}
                         triggeredByFormChange:false
                                       inFrame:frame];

  // Simulate user focusing the field in a form before the password store
  // response is received.
  FormSuggestionProviderQuery* form_query1 =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:SysUTF16ToNSString(form.name())
            formRendererID:form.renderer_id()
           fieldIdentifier:SysUTF16ToNSString(form.fields()[0].name())
           fieldRendererID:form.fields()[0].renderer_id()
                 fieldType:@"text"
                      type:@"focus"
                typedValue:@""
                   frameID:kTestFrameID];

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  __block BOOL completion_was_called1 = NO;
  [controller_ checkIfSuggestionsAvailableForForm:form_query1
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  completion_was_called1 = YES;
                                }];

  // Check that completion handler wasn't called.
  EXPECT_FALSE(completion_was_called1);

  // Simulate user focusing another field in a form before the password store
  // response is received.
  FormSuggestionProviderQuery* form_query2 =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:SysUTF16ToNSString(form.name())
            formRendererID:form.renderer_id()
           fieldIdentifier:SysUTF16ToNSString(form.fields()[1].name())
           fieldRendererID:form.fields()[1].renderer_id()
                 fieldType:kObfuscatedFieldType
                      type:@"focus"
                typedValue:@""
                   frameID:kTestFrameID];

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  __block BOOL completion_was_called2 = NO;
  [controller_ checkIfSuggestionsAvailableForForm:form_query2
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  completion_was_called2 = YES;
                                }];

  // Check that completion handler wasn't called yet, not until processing fill
  // data.
  EXPECT_FALSE(completion_was_called1);
  EXPECT_FALSE(completion_was_called2);

  // Receive suggestions from PasswordManager.
  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      form.url().spec(), "", form.renderer_id().value(), "",
      form.fields()[0].renderer_id().value(), "john.doe@gmail.com", "",
      form.fields()[1].renderer_id().value(), "super!secret", nullptr, nullptr,
      &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                forFrameId:web_frame_id
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  // Check that completion handlers were called.
  EXPECT_TRUE(completion_was_called1);
  EXPECT_TRUE(completion_was_called2);
}

// Test that the password suggestions for cross-origin iframes have the origin
// as their description.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       CrossOriginIframeSugesstionHasOriginAsDescription) {
  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      kTestURL, "", form.renderer_id().value(), "",
      form.fields()[0].renderer_id().value(), "john.doe@gmail.com", "",
      form.fields()[1].renderer_id().value(), "super!secret", nullptr, nullptr,
      &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                forFrameId:web_frame_id
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:kObfuscatedFieldType
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 // Assert that kTestURL contains [suggestions[0]
                 // displayDescription].
                 ASSERT_NE(kTestURL.find(SysNSStringToUTF8(
                               [suggestions[0] displayDescription])),
                           std::string::npos);
               }];
}

// Tests that attachListenersForBottomSheet, from the
// PasswordSuggestionHelperDelegate protocol, is properly used by the
// PasswordSuggestionHelper object.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       AttachListenersForBottomSheet) {
  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame = web::FakeWebFrame::Create(
      web_frame_id, /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];
  AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);

  [controller_ didFinishPasswordFormExtraction:{form}
                         triggeredByFormChange:false
                                       inFrame:frame];

  // Receive suggestions from PasswordManager.
  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      form.url().spec(), "", form.renderer_id().value(), "",
      form.fields()[0].renderer_id().value(), "john.doe@gmail.com", "",
      form.fields()[1].renderer_id().value(), "super!secret", nullptr, nullptr,
      &form_fill_data);

  std::vector<autofill::FieldRendererId> rendererIds;

  OCMExpect([[delegate_ ignoringNonObjectArgs]
                attachListenersForBottomSheet:rendererIds
                                   forFrameId:""])
      .andCompareStringAtIndex(web_frame_id, 1);

  [controller_ processPasswordFormFillData:form_fill_data
                                forFrameId:web_frame_id
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  [delegate_ verify];
}

// Tests frameDidBecomeAvailable supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       FrameDidBecomeAvailableCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  [[form_helper_ expect] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];

  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));
  [form_helper_ verify];
}

// Tests frameWillBecomeUnavailable supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       FrameWillBecomeUnavailableCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  //  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_manager_, OnIframeDetach).Times(1);
  web_frames_manager_->RemoveWebFrame(frame->GetFrameId());
}

// Tests checkIfSuggestionsAvailableForForm supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       CheckIfSuggestionsAvailableForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  id mock_completion_handler =
      [OCMArg checkWithBlock:^BOOL(void (^completionHandler)(BOOL)) {
        completionHandler(YES);
        return YES;
      }];

  [[suggestion_helper_ expect]
      checkIfSuggestionsAvailableForForm:form_query
                       completionHandler:mock_completion_handler];
  [[suggestion_helper_ expect] isPasswordFieldOnForm:form_query webFrame:frame];

  [controller_ checkIfSuggestionsAvailableForForm:form_query
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  EXPECT_TRUE(suggestionsAvailable);
                                }];
}

// Tests retrieveSuggestionsForForm supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       RetrieveSuggestionsForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  const std::string web_frame_id = SysNSStringToUTF8(kTestFrameID);
  auto web_frame =
      web::FakeWebFrame::Create(web_frame_id,
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
        formRendererID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
       fieldRendererID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(@[]);
  OCMExpect([[suggestion_helper_ ignoringNonObjectArgs]
      isPasswordFieldOnForm:form_query
                   webFrame:frame]);

  EXPECT_CALL(password_generation_helper_, IsGenerationEnabled(true))
      .WillOnce(Return(false));

  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate){
               }];
  [suggestion_helper_ verify];
}

// Tests formHelper didSubmitForm supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       FormHelperDidSubmitFormForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  EXPECT_CALL(password_manager_, OnSubframeFormSubmission).Times(1);

  FormData form_data;
  [controller_ formHelper:form_helper_ didSubmitForm:form_data inFrame:frame];
}

// Tests didRegisterFormActivity supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       DidRegisterFormActivityForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                 completionHandler:[OCMArg any]]);

  web_frames_manager_->AddWebFrame(std::move(web_frame));

  autofill::FormActivityParams params;
  params.type = "form_changed";

  OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                  completionHandler:[OCMArg any]]);

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:frame];

  [form_helper_ verify];
}

// Tests didRegisterFormRemoval supports cross-origin iframes.
TEST_F(SharedPasswordControllerTest,
       DidRegisterFormRemovalForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_manager_, OnPasswordFormsRemoved).Times(1);

  autofill::FormRemovalParams params;
  params.removed_forms = {autofill::FormRendererId()};

  [controller_ webState:&web_state_
      didRegisterFormRemoval:params
                     inFrame:frame];
}

// Tests that password generation is terminated correctly when the
// user declines the dialog.
TEST_F(SharedPasswordControllerTest, DeclinePasswordGenerationDialog) {
  base::HistogramTester histogram_tester;

  autofill::FormRendererId form_id(0);
  autofill::FieldRendererId field_id(1);
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, field_id, field_id};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  AddWebFrame(std::move(web_frame));

  // Create a password generation suggestion.
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
                     type:autofill::SuggestionType::kGeneratePasswordEntry
        backendIdentifier:nil
           requiresReauth:NO];

  // Triggering password generation will trigger a new form extraction.
  // Simulate it completes successfully.
  FormData form_data = test_helpers::MakeSimpleFormData();
  id extract_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL, FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg];

  // Simulate the user declining the generated password in the dialog.
  id decision_handler_arg =
      [OCMArg checkWithBlock:^(void (^decision_handler)(BOOL)) {
        decision_handler(/*accept=*/NO);
        return YES;
      }];
  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                                     proactive:NO
                               decisionHandler:decision_handler_arg];

  OCMStub([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, GeneratePassword)
      .WillOnce(Return(u"testpass"));
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  // Check that the generation is terminated.
  EXPECT_CALL(password_manager_, OnPasswordNoLongerGenerated);

  // Start the password generation flow by selecting the password generation
  // suggestion. The flow will complete itself by declining the generated
  // password.
  [controller_ didSelectSuggestion:suggestion
                           atIndex:0
                              form:@"test-form-name"
                    formRendererID:form_id
                   fieldIdentifier:@"test-field-id"
                   fieldRendererID:field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  // Verify that the metric is only recorded iff the generated password is
  // accepted.
  histogram_tester.ExpectTotalCount(
      "PasswordGeneration.iOS.AcceptedGeneratedPasswordIsEmpty", 0);
}

// Tests that upon calling DidFillField() on the agent, the delegate implemented
// and owned by the SharedPasswordController correctly calls the password
// manager to update its state.
TEST_F(SharedPasswordControllerTest, DidFillField) {
  GURL url("https://example.com");
  auto frame = web::FakeWebFrame::Create("frameID", true, url);
  autofill::FormRendererId form_id(1);
  autofill::FieldRendererId field_id(2);
  const std::u16string value(u"value");
  auto* driver = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
      &web_state_, frame.get());
  auto* field_data_manager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame.get());

  EXPECT_CALL(
      password_manager_,
      UpdateStateOnUserInput(driver, ::testing::Ref(*field_data_manager),
                             std::make_optional<FormRendererId>(form_id),
                             field_id, value));

  auto* agent = autofill::PasswordAutofillAgent::FromWebState(&web_state_);
  agent->DidFillField(frame.get(), form_id, field_id, value);
}

// TODO(crbug.com/40701292): Finish unit testing the rest of the public API.

}  // namespace password_manager
