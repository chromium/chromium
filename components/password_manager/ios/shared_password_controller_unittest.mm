// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/shared_password_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "components/password_manager/ios/shared_password_controller+private.h"
#include "components/password_manager/ios/test_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using password_manager::IsCrossOriginIframe;
using password_manager::PasswordGenerationFrameHelper;
using ::testing::_;
using ::testing::Return;

namespace password_manager {

namespace {

const std::string kTestURL = "https://www.chromium.org/";
NSString* kTestFrameID = @"dummy-frame-id";

}  // namespace
class MockPasswordManager : public PasswordManagerInterface {
 public:
  MOCK_METHOD(void, DidNavigateMainFrame, (bool), (override));
  MOCK_METHOD(void,
              OnPasswordFormsParsed,
              (PasswordManagerDriver*, const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormsRendered,
              (PasswordManagerDriver*, const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmitted,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormCleared,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              SetGenerationElementAndTypeForForm,
              (PasswordManagerDriver*,
               autofill::FormRendererId,
               autofill::FieldRendererId,
               autofill::password_generation::PasswordGenerationType),
              (override));
  MOCK_METHOD(void,
              OnPresaveGeneratedPassword,
              (PasswordManagerDriver*,
               const autofill::FormData&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              OnSubframeFormSubmission,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              UpdateStateOnUserInput,
              (PasswordManagerDriver*,
               autofill::FormRendererId,
               autofill::FieldRendererId,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              OnPasswordNoLongerGenerated,
              (PasswordManagerDriver*),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormRemoved,
              (PasswordManagerDriver*,
               const autofill::FieldDataManager&,
               autofill::FormRendererId),
              (override));
  MOCK_METHOD(void,
              OnIframeDetach,
              (const std::string&,
               PasswordManagerDriver*,
               const autofill::FieldDataManager&),
              (override));
  MOCK_METHOD(void,
              PropagateFieldDataManagerInfo,
              (const autofill::FieldDataManager&, const PasswordManagerDriver*),
              (override));
  MOCK_METHOD(PasswordManagerClient*, GetClient, (), (override));
};

class MockPasswordGenerationFrameHelper : public PasswordGenerationFrameHelper {
 public:
  MOCK_METHOD(std::u16string,
              GeneratePassword,
              (const GURL&,
               autofill::FormSignature,
               autofill::FieldSignature,
               uint32_t),
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

    controller_ =
        [[SharedPasswordController alloc] initWithWebState:&web_state_
                                                   manager:&password_manager_
                                                formHelper:form_helper_
                                          suggestionHelper:suggestion_helper_
                                              driverHelper:driver_helper_];
    controller_.delegate = delegate_;
    [suggestion_helper_ verify];
    [form_helper_ verify];
    UniqueIDDataTabHelper::CreateForWebState(&web_state_);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));

    web_state_.SetCurrentURL(GURL(kTestURL));
  }

  void SetUp() override {
    PlatformTest::SetUp();

    PasswordGenerationFrameHelper* password_generation_helper_ptr =
        &password_generation_helper_;
    [[[driver_helper_ stub]
        andReturnValue:OCMOCK_VALUE(password_generation_helper_ptr)]
        PasswordGenerationHelper:static_cast<web::WebFrame*>(
                                     [OCMArg anyPointer])];

    EXPECT_CALL(password_manager_, GetClient)
        .WillRepeatedly(Return(&password_manager_client_));
  }

 protected:
  autofill::test::AutofillEnvironment autofill_environment_;
  web::FakeWebState web_state_;
  web::FakeWebFramesManager* web_frames_manager_;
  testing::StrictMock<MockPasswordManager> password_manager_;
  testing::StrictMock<MockPasswordGenerationFrameHelper>
      password_generation_helper_;
  id form_helper_;
  id suggestion_helper_;
  id driver_helper_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  id delegate_;
  SharedPasswordController* controller_;
};

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
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, PropagateFieldDataManagerInfo);
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
  web_frames_manager_->AddWebFrame(std::move(web_frame));
  [[[form_helper_ expect] ignoringNonObjectArgs]
      setUpForUniqueIDsWithInitialState:1
                                inFrame:frame];

  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(
          const std::vector<autofill::FormData>& forms, uint32_t maxID)) {
        OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
        EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
        EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
        OCMExpect([suggestion_helper_ updateStateOnPasswordFormExtracted]);
        autofill::FormData form_data = test_helpers::MakeSimpleFormData();
        completionHandler({form_data}, 1);
        return YES;
      }];
  OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                 completionHandler:mock_completion_handler]);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  web_state_.OnWebFrameDidBecomeAvailable(frame);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests form finding and parsing is not triggered for non HTML pages.
TEST_F(SharedPasswordControllerTest, NoFormsArePropagatedOnNonHTMLPageLoad) {
  web_state_.SetCurrentURL(GURL(kTestURL));
  web_state_.SetContentIsHTML(false);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [[form_helper_ reject] findPasswordFormsInFrame:frame
                                completionHandler:[OCMArg any]];
  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  OCMExpect([suggestion_helper_ processWithNoSavedCredentials]);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [suggestion_helper_ verify];
  [form_helper_ verify];
}

// Tests that new frames will trigger PasswordFormHelper to set up unique IDs.
TEST_F(SharedPasswordControllerTest, FormHelperSetsUpUniqueIDsForNewFrame) {
  auto web_frame =
      web::FakeWebFrame::Create("dummy-frame-id",
                                /*is_main_frame=*/true, GURL(kTestURL));
  [[[form_helper_ expect] ignoringNonObjectArgs]
      setUpForUniqueIDsWithInitialState:1
                                inFrame:web_frame.get()];
  OCMExpect([form_helper_ findPasswordFormsInFrame:web_frame.get()
                                 completionHandler:[OCMArg any]]);
  web_state_.OnWebFrameDidBecomeAvailable(web_frame.get());
}

// Tests that suggestions are reported as unavailable for nonpassword forms.
TEST_F(SharedPasswordControllerTest,
       CheckNoSuggestionsAreAvailableForNonPasswordForm) {
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  id mock_completion_handler =
      [OCMArg checkWithBlock:^BOOL(void (^completionHandler)(BOOL)) {
        // Ensure |suggestion_helper_| reports no suggestions.
        completionHandler(NO);
        return YES;
      }];
  [[suggestion_helper_ expect]
      checkIfSuggestionsAvailableForForm:form_query
                       completionHandler:mock_completion_handler];

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
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];
  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [[[suggestion_helper_ expect] andReturn:@[]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                            inFrame:frame
                          fieldType:form_query.fieldType];

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
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  web::WebFrame* frame = nullptr;

  [[[suggestion_helper_ expect] andReturn:@[]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                            inFrame:frame
                          fieldType:form_query.fieldType];

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
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:@"value"
                       displayDescription:@"display-description"
                                     icon:@"icon"
                               identifier:0
                           requiresReauth:NO];

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [[[suggestion_helper_ expect] andReturn:@[ suggestion ]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                            inFrame:frame
                          fieldType:form_query.fieldType];

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
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [[[suggestion_helper_ expect] andReturn:@[]]
      retrieveSuggestionsWithFormID:form_query.uniqueFormID
                    fieldIdentifier:form_query.uniqueFieldID
                            inFrame:frame
                          fieldType:form_query.fieldType];

  autofill::PasswordFormGenerationData form_generation_data = {
      form_query.uniqueFormID, form_query.uniqueFieldID,
      form_query.uniqueFieldID};
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
                 EXPECT_EQ(autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY,
                           suggestion.identifier);
                 EXPECT_EQ(delegate, controller_);
                 completion_was_called = YES;
               }];
  EXPECT_TRUE(completion_was_called);
}

// Tests that accepting a "Suggest a password" suggestion will give a suggested
// password to the delegate.
TEST_F(SharedPasswordControllerTest, SuggestsGeneratedPassword) {
  GURL origin("https://www.google.com/login");
  const uint64_t max_length = 10;

  autofill::FormData form_data;
  form_data.url = origin;
  form_data.action = origin;
  form_data.name = u"login_form";
  form_data.unique_renderer_id = autofill::test::MakeFormRendererId();

  autofill::FormFieldData field;
  field.name = u"Username";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = u"googleuser";
  field.form_control_type = "text";
  field.unique_renderer_id = autofill::test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  field.name = u"Passwd";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = u"p4ssword";
  field.form_control_type = "password";
  field.unique_renderer_id = autofill::test::MakeFieldRendererId();
  field.max_length = max_length;
  form_data.fields.push_back(field);

  autofill::FormFieldData password_field_data = form_data.fields.back();
  autofill::FormRendererId form_id = form_data.unique_renderer_id;
  autofill::FieldRendererId field_id = password_field_data.unique_renderer_id;
  autofill::PasswordFormGenerationData form_generation_data = {
      form_id, field_id,
      /*confirmation_field=*/autofill::FieldRendererId(0)};
  [controller_ formEligibleForGenerationFound:form_generation_data];

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
               identifier:autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY
           requiresReauth:NO];

  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:[OCMArg any]];
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  id extract_completion_handler_arg = [OCMArg
      checkWithBlock:^(void (^completion_handler)(BOOL, autofill::FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg];

  // Verify the parameters that |GeneratePassword| receives.
  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureForField(password_field_data);

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_,
              GeneratePassword(web_state_.GetLastCommittedURL(), form_signature,
                               field_signature, max_length));

  [controller_ didSelectSuggestion:suggestion
                              form:@"test-form-name"
                      uniqueFormID:form_id
                   fieldIdentifier:@"test-field-id"
                     uniqueFieldID:field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  [delegate_ verify];
}

// Tests that generated passwords are presaved.
TEST_F(SharedPasswordControllerTest, PresavesGeneratedPassword) {
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
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test-value"
       displayDescription:@"test-description"
                     icon:nil
               identifier:autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY
           requiresReauth:NO];

  id decision_handler_arg =
      [OCMArg checkWithBlock:^(void (^decision_handler)(BOOL)) {
        decision_handler(/*accept=*/YES);
        return YES;
      }];
  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:decision_handler_arg];

  id fill_completion_handler_arg =
      [OCMArg checkWithBlock:^(void (^completion_handler)(BOOL)) {
        completion_handler(/*success=*/YES);
        return YES;
      }];
  [[form_helper_ expect] fillPasswordForm:form_id
                                  inFrame:frame
                    newPasswordIdentifier:field_id
                confirmPasswordIdentifier:field_id
                        generatedPassword:[OCMArg isNotNil]
                        completionHandler:fill_completion_handler_arg];

  autofill::FormData form_data = test_helpers::MakeSimpleFormData();
  id extract_completion_handler_arg = [OCMArg
      checkWithBlock:^(void (^completion_handler)(BOOL, autofill::FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg];
  OCMStub([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, GeneratePassword);

  EXPECT_CALL(password_manager_, OnPresaveGeneratedPassword);
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  [controller_ didSelectSuggestion:suggestion
                              form:@"test-form-name"
                      uniqueFormID:form_id
                   fieldIdentifier:@"test-field-id"
                     uniqueFieldID:field_id
                           frameID:kTestFrameID
                 completionHandler:nil];

  [delegate_ verify];
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_ACCEPTED, 1);
}

// Tests that triggering password generation on the last focused field triggers
// the generation flow.
TEST_F(SharedPasswordControllerTest, TriggerPasswordGeneration) {
  base::HistogramTester histogram_tester;
  autofill::FormActivityParams params;
  params.unique_form_id = autofill::FormRendererId(0);
  params.field_type = "password";
  params.unique_field_id = autofill::FieldRendererId(1);
  params.type = "focus";
  params.input_missing = false;

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:frame];

  [[delegate_ expect] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:[OCMArg any]];
  EXPECT_CALL(password_manager_, SetGenerationElementAndTypeForForm);

  autofill::FormData form_data = test_helpers::MakeSimpleFormData();
  id extract_completion_handler_arg = [OCMArg
      checkWithBlock:^(void (^completion_handler)(BOOL, autofill::FormData)) {
        completion_handler(/*found=*/YES, form_data);
        return YES;
      }];
  [[form_helper_ expect]
      extractPasswordFormData:params.unique_form_id
                      inFrame:frame
            completionHandler:extract_completion_handler_arg];
  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
  EXPECT_CALL(password_generation_helper_, GeneratePassword);

  [controller_ triggerPasswordGeneration];

  [delegate_ verify];
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED,
      1);
}

// Tests that triggering password generation on the last focused field does not
// trigger the generation flow if the last reported form activity did not
// provide valid form and field identifiers.
TEST_F(SharedPasswordControllerTest, LastFocusedFieldData) {
  autofill::FormActivityParams params;
  params.unique_form_id = autofill::FormRendererId(0);
  params.field_type = "password";
  params.unique_field_id = autofill::FieldRendererId(1);
  params.type = "focus";
  params.input_missing = true;

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:web_frame.get()];

  [[delegate_ reject] sharedPasswordController:controller_
                showGeneratedPotentialPassword:[OCMArg isNotNil]
                               decisionHandler:[OCMArg any]];

  [controller_ triggerPasswordGeneration];

  [delegate_ verify];
}

// Tests that detecting element additions (form_changed events) is not
// interpreted as form submissions.
TEST_F(SharedPasswordControllerTest,
       DoesntInterpretElementAdditionsAsFormSubmission) {
  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(
          const std::vector<autofill::FormData>& forms, uint32_t maxID)) {
        EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
        // OnPasswordFormsRendered is responsible for detecting submissions.
        // Making sure it's not called after element additions.
        EXPECT_CALL(password_manager_, OnPasswordFormsRendered).Times(0);
        OCMExpect([suggestion_helper_ updateStateOnPasswordFormExtracted]);
        autofill::FormData form_data = test_helpers::MakeSimpleFormData();
        completionHandler({form_data}, 1);
        return YES;
      }];

  auto web_frame = web::FakeWebFrame::Create("frame-id", /*is_main_frame=*/true,
                                             GURL(kTestURL));
  web::FakeWebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                 completionHandler:mock_completion_handler]);

  autofill::FormActivityParams params;
  params.type = "form_changed";

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

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

    UniqueIDDataTabHelper::CreateForWebState(&web_state_);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));

    web_state_.SetCurrentURL(GURL(kTestURL));
  }

  void SetUp() override {
    PlatformTest::SetUp();

    EXPECT_CALL(password_manager_, GetClient)
        .WillRepeatedly(Return(&password_manager_client_));
  }

 protected:
  web::FakeWebState web_state_;
  web::FakeWebFramesManager* web_frames_manager_;
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

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);

  [controller_ didFinishPasswordFormExtraction:{form}
                               withMaxUniqueID:5
                         triggeredByFormChange:false
                                       inFrame:frame];

  // Simulate user focusing the field in a form before the password store
  // response is received.
  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:SysUTF16ToNSString(form.name)
          uniqueFormID:form.unique_renderer_id
       fieldIdentifier:SysUTF16ToNSString(form.fields[0].name)
         uniqueFieldID:form.fields[0].unique_renderer_id
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

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
      form.url.spec(), "", form.unique_renderer_id.value(), "",
      form.fields[0].unique_renderer_id.value(), "john.doe@gmail.com", "",
      form.fields[1].unique_renderer_id.value(), "super!secret", nullptr,
      nullptr, &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                   inFrame:frame
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  // Check that completion handler was called.
  EXPECT_TRUE(completion_was_called);
}

// Tests the completion handler for suggestions availability is not called
// until password manager replies with suggestions.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       WaitForPasswordmanagerResponseToShowSuggestionsTwoFields) {
  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/true, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  EXPECT_CALL(password_manager_, OnPasswordFormsParsed);
  EXPECT_CALL(password_manager_, OnPasswordFormsRendered);

  [controller_ didFinishPasswordFormExtraction:{form}
                               withMaxUniqueID:5
                         triggeredByFormChange:false
                                       inFrame:frame];

  // Simulate user focusing the field in a form before the password store
  // response is received.
  FormSuggestionProviderQuery* form_query1 =
      [[FormSuggestionProviderQuery alloc]
          initWithFormName:SysUTF16ToNSString(form.name)
              uniqueFormID:form.unique_renderer_id
           fieldIdentifier:SysUTF16ToNSString(form.fields[0].name)
             uniqueFieldID:form.fields[0].unique_renderer_id
                 fieldType:@"text"
                      type:@"focus"
                typedValue:@""
                   frameID:kTestFrameID];

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
          initWithFormName:SysUTF16ToNSString(form.name)
              uniqueFormID:form.unique_renderer_id
           fieldIdentifier:SysUTF16ToNSString(form.fields[1].name)
             uniqueFieldID:form.fields[1].unique_renderer_id
                 fieldType:@"password"
                      type:@"focus"
                typedValue:@""
                   frameID:kTestFrameID];

  __block BOOL completion_was_called2 = NO;
  [controller_ checkIfSuggestionsAvailableForForm:form_query2
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  completion_was_called2 = YES;
                                }];

  // Check that completion handler wasn't called.
  EXPECT_FALSE(completion_was_called2);

  // Receive suggestions from PasswordManager.
  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      form.url.spec(), "", form.unique_renderer_id.value(), "",
      form.fields[0].unique_renderer_id.value(), "john.doe@gmail.com", "",
      form.fields[1].unique_renderer_id.value(), "super!secret", nullptr,
      nullptr, &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                   inFrame:frame
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  // Check that completion handler was called for the second form query.
  EXPECT_FALSE(completion_was_called1);
  EXPECT_TRUE(completion_was_called2);
}

// Test that the password suggestions for cross-origin iframes have the origin
// as their description.
TEST_F(SharedPasswordControllerTestWithRealSuggestionHelper,
       CrossOriginIframeSugesstionHasOriginAsDescription) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kIOSPasswordManagerCrossOriginIframeSupport);

  // Simulate that the form is parsed and sent to PasswordManager.
  FormData form = test_helpers::MakeSimpleFormData();

  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      kTestURL, "", form.unique_renderer_id.value(), "",
      form.fields[0].unique_renderer_id.value(), "john.doe@gmail.com", "",
      form.fields[1].unique_renderer_id.value(), "super!secret", nullptr,
      nullptr, &form_fill_data);

  [controller_ processPasswordFormFillData:form_fill_data
                                   inFrame:frame
                               isMainFrame:frame->IsMainFrame()
                         forSecurityOrigin:frame->GetSecurityOrigin()];

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:kPasswordFieldType
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

class SharedPasswordControllerTestCrossOrigin
    : public SharedPasswordControllerTest,
      public testing::WithParamInterface<bool> {
 public:
  SharedPasswordControllerTestCrossOrigin() : SharedPasswordControllerTest() {
    if (IsCrossOriginSupportEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kIOSPasswordManagerCrossOriginIframeSupport},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              features::kIOSPasswordManagerCrossOriginIframeSupport});
    }
  }
  bool IsCrossOriginSupportEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests frameDidBecomeAvailable supports cross-origin iframes when the feature
// is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       FrameDidBecomeAvailableCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  [[[form_helper_ expect] ignoringNonObjectArgs]
      setUpForUniqueIDsWithInitialState:1
                                inFrame:frame];

  if (IsCrossOriginSupportEnabled()) {
    [[form_helper_ expect] findPasswordFormsInFrame:frame
                                  completionHandler:[OCMArg any]];
  } else {
    [[form_helper_ reject] findPasswordFormsInFrame:frame
                                  completionHandler:[OCMArg any]];
  }
  [controller_ webState:&web_state_ frameDidBecomeAvailable:frame];
  [form_helper_ verify];
}

// Tests frameWillBecomeUnavailable supports cross-origin iframes when the
// feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       FrameWillBecomeUnavailableCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  if (IsCrossOriginSupportEnabled()) {
    OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
    EXPECT_CALL(password_manager_, OnIframeDetach).Times(1);
  } else {
    EXPECT_CALL(password_manager_, OnIframeDetach).Times(0);
  }
  [controller_ webState:&web_state_ frameWillBecomeUnavailable:frame];
}

// Tests checkIfSuggestionsAvailableForForm supports cross-origin iframes when
// the feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       CheckIfSuggestionsAvailableForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
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

  [controller_ checkIfSuggestionsAvailableForForm:form_query
                                   hasUserGesture:NO
                                         webState:&web_state_
                                completionHandler:^(BOOL suggestionsAvailable) {
                                  if (IsCrossOriginSupportEnabled()) {
                                    EXPECT_TRUE(suggestionsAvailable);
                                  } else {
                                    EXPECT_FALSE(suggestionsAvailable);
                                  }
                                }];
}

// Tests retrieveSuggestionsForForm supports cross-origin iframes when the
// feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       RetrieveSuggestionsForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  FormSuggestionProviderQuery* form_query = [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"form"
          uniqueFormID:autofill::FormRendererId(0)
       fieldIdentifier:@"field"
         uniqueFieldID:autofill::FieldRendererId(1)
             fieldType:@"text"
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID];

  if (IsCrossOriginSupportEnabled()) {
    [[[suggestion_helper_ expect] andReturn:@[]]
        retrieveSuggestionsWithFormID:form_query.uniqueFormID
                      fieldIdentifier:form_query.uniqueFieldID
                              inFrame:frame
                            fieldType:form_query.fieldType];

    EXPECT_CALL(password_generation_helper_, IsGenerationEnabled(true))
        .WillOnce(Return(false));
  } else {
    [[suggestion_helper_ reject]
        retrieveSuggestionsWithFormID:form_query.uniqueFormID
                      fieldIdentifier:form_query.uniqueFieldID
                              inFrame:frame
                            fieldType:form_query.fieldType];

    EXPECT_CALL(password_manager_, OnIframeDetach).Times(0);
  }

  [controller_
      retrieveSuggestionsForForm:form_query
                        webState:&web_state_
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate){
               }];
  [suggestion_helper_ verify];
}

// Tests formHelper didSubmitForm supports cross-origin iframes when the
// feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       FormHelperDidSubmitFormForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  OCMExpect([driver_helper_ PasswordManagerDriver:frame]);

  if (IsCrossOriginSupportEnabled()) {
    EXPECT_CALL(password_manager_, OnSubframeFormSubmission).Times(1);
  } else {
    EXPECT_CALL(password_manager_, OnSubframeFormSubmission).Times(0);
  }

  autofill::FormData form_data;
  [controller_ formHelper:form_helper_ didSubmitForm:form_data inFrame:frame];
}

// Tests didRegisterFormActivity supports cross-origin iframes when the
// feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       DidRegisterFormActivityForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(
          const std::vector<autofill::FormData>& forms, uint32_t maxID)) {
        return YES;
      }];

  if (IsCrossOriginSupportEnabled()) {
    OCMExpect([form_helper_ findPasswordFormsInFrame:frame
                                   completionHandler:mock_completion_handler]);
  } else {
    [[form_helper_ reject] findPasswordFormsInFrame:frame
                                  completionHandler:mock_completion_handler];
  }

  autofill::FormActivityParams params;
  params.type = "form_changed";

  [controller_ webState:&web_state_
      didRegisterFormActivity:params
                      inFrame:frame];

  [form_helper_ verify];
}

// Tests didRegisterFormRemoval supports cross-origin iframes when the
// feature is enabled and disabled.
TEST_P(SharedPasswordControllerTestCrossOrigin,
       DidRegisterFormRemovalForFormCrossOriginIframe) {
  web_state_.SetCurrentURL(GURL());
  web_state_.SetContentIsHTML(true);

  auto web_frame =
      web::FakeWebFrame::Create(SysNSStringToUTF8(kTestFrameID),
                                /*is_main_frame=*/false, GURL(kTestURL));
  web::WebFrame* frame = web_frame.get();
  web_frames_manager_->AddWebFrame(std::move(web_frame));

  ASSERT_TRUE(IsCrossOriginIframe(&web_state_, frame->IsMainFrame(),
                                  frame->GetSecurityOrigin()));

  if (IsCrossOriginSupportEnabled()) {
    OCMExpect([driver_helper_ PasswordManagerDriver:frame]);
    EXPECT_CALL(password_manager_, OnPasswordFormRemoved).Times(1);
  } else {
    EXPECT_CALL(password_manager_, OnPasswordFormRemoved).Times(0);
  }

  autofill::FormRendererId unique_form_id;
  autofill::FormRemovalParams params;
  params.unique_form_id = unique_form_id;

  [controller_ webState:&web_state_
      didRegisterFormRemoval:params
                     inFrame:frame];
}

INSTANTIATE_TEST_SUITE_P(,
                         SharedPasswordControllerTestCrossOrigin,
                         testing::Bool());

// TODO(crbug.com/1097353): Finish unit testing the rest of the public API.

}  // namespace password_manager
