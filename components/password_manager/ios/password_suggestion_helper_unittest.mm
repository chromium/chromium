// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_suggestion_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/test_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using autofill::test::MakeFieldRendererId;
using autofill::test::MakeFormRendererId;
using base::SysUTF8ToNSString;
using base::UTF8ToUTF16;

namespace {

constexpr char kTestUrl[] = "http://foo.com";
constexpr char kFillDataUsername[] = "john.doe@gmail.com";
constexpr char kFillDataPassword[] = "super!secret";
NSString* const kTestFrameID = @"mainframe";
NSString* const kTextFieldType = @"text";
NSString* const kQueryFocusType = @"focus";

NSString* NSFrameId(web::WebFrame* frame) {
  return SysUTF8ToNSString(frame->GetFrameId());
}

PasswordFormFillData CreatePasswordFillData(
    FormRendererId form_renderer_id,
    FieldRendererId username_renderer_id,
    FieldRendererId password_renderer_id) {
  PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      kTestUrl, "", form_renderer_id.value(), "", username_renderer_id.value(),
      kFillDataUsername, "", password_renderer_id.value(), kFillDataPassword,
      nullptr, nullptr, &form_fill_data);
  return form_fill_data;
}

}  // namespace

class PasswordSuggestionHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    web_state_.SetCurrentURL(GURL(kTestUrl));

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager_ = frames_manager.get();

    auto main_frame =
        web::FakeWebFrame::Create("mainframe", true, GURL(kTestUrl));
    main_frame_ = main_frame.get();
    AddWebFrame(std::move(main_frame));

    web::ContentWorld content_world =
        ContentWorldForAutofillJavascriptFeatures();
    web_state_.SetWebFramesManager(content_world, std::move(frames_manager));

    autofill::AutofillDriverIOSFactory::CreateForWebState(
        &web_state_, &autofill_client_, /*autofill_agent=*/nil,
        /*locale=*/"en");

    delegate_ = OCMProtocolMock(@protocol(PasswordSuggestionHelperDelegate));

    helper_ = [[PasswordSuggestionHelper alloc] initWithWebState:&web_state_];
    helper_.delegate = delegate_;
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    frames_manager_->AddWebFrame(std::move(frame));
  }

  FormSuggestionProviderQuery* BuildQuery(
      autofill::FormRendererId formRendererID,
      NSString* fieldIdentifier,
      autofill::FieldRendererId fieldRendererID,
      NSString* fieldType,
      NSString* frameID) {
    return [[FormSuggestionProviderQuery alloc] initWithFormName:@"form1"
                                                  formRendererID:formRendererID
                                                 fieldIdentifier:fieldIdentifier
                                                 fieldRendererID:fieldRendererID
                                                       fieldType:fieldType
                                                            type:kQueryFocusType
                                                      typedValue:@""
                                                         frameID:frameID];
  }

  FormSuggestionProviderQuery* BuildPasswordQuery(
      autofill::FormRendererId formRendererID,
      autofill::FieldRendererId fieldRendererID,
      NSString* frameID) {
    return BuildQuery(formRendererID, @"password1", fieldRendererID,
                      kObfuscatedFieldType, frameID);
  }

  FormSuggestionProviderQuery* BuildQuery(NSString* fieldIdentifier,
                                          NSString* fieldType,
                                          NSString* frameID) {
    return BuildQuery(autofill::test::MakeFormRendererId(), fieldIdentifier,
                      autofill::test::MakeFieldRendererId(), fieldType,
                      frameID);
  }

  web::WebTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClient autofill_client_;
  web::FakeWebState web_state_;
  id delegate_;
  PasswordSuggestionHelper* helper_;
  raw_ptr<web::FakeWebFrame> main_frame_;
  raw_ptr<web::FakeWebFramesManager> frames_manager_;
};

// Tests that the suggestions check query passes when there is fill data for the
// password field in the form that is being checked.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_WithFillDataImmediately_OnPasswordField) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"pwd1", kObfuscatedFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = autofill::test::MakeFieldRendererId();
  FieldRendererId password1_renderer_id = query.fieldRendererID;

  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:main_frame_->GetFrameId()]);
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  __block BOOL retrieved_suggestions = NO;
  __block BOOL completion_called = NO;
  SuggestionsAvailableCompletion completion = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions = suggestionsAvailable;
    completion_called = YES;
  };
  OCMReject([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);
  [helper_ checkIfSuggestionsAvailableForForm:query
                            completionHandler:completion];

  ASSERT_EQ(YES, completion_called);

  EXPECT_EQ(YES, retrieved_suggestions);
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that the suggestions check query passes when there is fill data for the
// username field in the form that is being checked.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_WithFillDataImmediately_OnUsernameField) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = query.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();

  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:main_frame_->GetFrameId()]);
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  __block BOOL retrieved_suggestions = NO;
  __block BOOL completion_called = NO;
  SuggestionsAvailableCompletion completion = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions = suggestionsAvailable;
    completion_called = YES;
  };
  OCMReject([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);
  [helper_ checkIfSuggestionsAvailableForForm:query
                            completionHandler:completion];

  ASSERT_EQ(YES, completion_called);

  EXPECT_EQ(YES, retrieved_suggestions);
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that the suggestions check query fails when there is no fill data for
// the field in the form that is being checked, not even with the triggered
// forms extraction.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_WithoutFillDataOnTriggeredFormsExtraction) {
  __block BOOL retrieved_suggestions = NO;
  __block BOOL completion_called = NO;
  SuggestionsAvailableCompletion completion = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions = suggestionsAvailable;
    completion_called = YES;
  };

  FormSuggestionProviderQuery* query =
      BuildQuery(@"pwd1", kObfuscatedFieldType, NSFrameId(main_frame_));
  OCMExpect([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);
  [helper_ checkIfSuggestionsAvailableForForm:query
                            completionHandler:completion];

  EXPECT_OCMOCK_VERIFY(delegate_);

  // The completion shouldn't be called yet as there is no fill data for the
  // queried form. The helper triggers forms extraction in an attempt to get
  // fill data for the form.
  ASSERT_EQ(NO, completion_called);

  // Give feedback to the helper that no saved credentials could be retrieved,
  // with the triggered forms extraction.
  [helper_ processWithNoSavedCredentialsWithFrameId:main_frame_->GetFrameId()];

  // Now the completion should be called since the triggered forms extraction
  // was done.
  ASSERT_EQ(YES, completion_called);

  EXPECT_EQ(NO, retrieved_suggestions);
}

// Tests that the suggestions check query passes when there is fill data for the
// field in the form that is being checked, after the triggered forms
// extraction.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_WithFillDataOnTriggeredFormsExtraction) {
  __block BOOL retrieved_suggestions = NO;
  __block BOOL completion_called = NO;
  SuggestionsAvailableCompletion completion = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions = suggestionsAvailable;
    completion_called = YES;
  };

  FormSuggestionProviderQuery* query =
      BuildQuery(@"pwd1", kObfuscatedFieldType, kTestFrameID);
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = autofill::test::MakeFieldRendererId();
  FieldRendererId password1_renderer_id = query.fieldRendererID;
  OCMExpect([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);
  [helper_ checkIfSuggestionsAvailableForForm:query
                            completionHandler:completion];

  // The completion shouldn't be called yet as there is no fill data for the
  // queried form. The helper triggers forms extraction in an attempt to get
  // fill data for the form.
  ASSERT_EQ(NO, completion_called);

  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:main_frame_->GetFrameId()]);

  // Process fill data for the queried form after the check query call. Will run
  // the queued queries.
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  // Now the completion should be called since the triggered forms extraction
  // was done.
  ASSERT_EQ(YES, completion_called);

  EXPECT_EQ(YES, retrieved_suggestions);

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that multiple queued check queries across forms are completed correctly
// with the right result after processing fill data.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_MultipleQueries_AcrossForms) {
  OCMExpect([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);

  // First query.
  __block BOOL retrieved_suggestions1 = NO;
  __block BOOL completion1_called = NO;
  SuggestionsAvailableCompletion completion1 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions1 = suggestionsAvailable;
    completion1_called = YES;
  };

  FormSuggestionProviderQuery* query1 =
      BuildQuery(@"password1", kObfuscatedFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query1.formRendererID;
  FormRendererId form2_renderer_id = autofill::test::MakeFormRendererId();
  FieldRendererId username1_renderer_id = query1.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();
  [helper_ checkIfSuggestionsAvailableForForm:query1
                            completionHandler:completion1];

  // Second query for the same focused field as query 1.
  __block BOOL retrieved_suggestions2 = NO;
  __block BOOL completion2_called = NO;
  SuggestionsAvailableCompletion completion2 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions2 = suggestionsAvailable;
    completion2_called = YES;
  };
  FormSuggestionProviderQuery* query2 = BuildPasswordQuery(
      form1_renderer_id, password1_renderer_id, NSFrameId(main_frame_));
  [helper_ checkIfSuggestionsAvailableForForm:query2
                            completionHandler:completion2];

  // Third query for a field in a different form.
  __block BOOL retrieved_suggestions3 = NO;
  __block BOOL completion3_called = NO;
  SuggestionsAvailableCompletion completion3 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions3 = suggestionsAvailable;
    completion3_called = YES;
  };
  FormSuggestionProviderQuery* query3 = BuildPasswordQuery(
      form2_renderer_id, password1_renderer_id, NSFrameId(main_frame_));
  // Queue the third query to check if suggestions are available. At this point
  // there are 4 queries in the queue to be executed the next time fill data is
  // processed.
  [helper_ checkIfSuggestionsAvailableForForm:query3
                            completionHandler:completion3];

  // The completions shouldn't be called yet as there is no fill data for the
  // queried forms. The helper triggers a  in an attempt to get fill
  // data for the form.
  ASSERT_EQ(NO, completion1_called);
  ASSERT_EQ(NO, completion2_called);
  ASSERT_EQ(NO, completion3_called);

  // Process fill data for the queried form after the check query call. Will run
  // the queued queries.
  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:main_frame_->GetFrameId()]);
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  // Now the queued completion blocks should be called since the triggered forms
  // extraction was done.
  ASSERT_EQ(YES, completion1_called);
  ASSERT_EQ(YES, completion2_called);
  ASSERT_EQ(YES, completion3_called);

  EXPECT_EQ(YES, retrieved_suggestions1);
  EXPECT_EQ(YES, retrieved_suggestions2);
  // Expect no suggestions for the third query because there was no fill data
  // for the queried form2.
  EXPECT_EQ(NO, retrieved_suggestions3);

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that multiple queued check queries across frames are completed
// correctly with the right result after processing fill data.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_MultipleQueries_AcrossFrames) {
  FormRendererId form1_renderer_id = autofill::test::MakeFormRendererId();
  FieldRendererId username1_renderer_id = autofill::test::MakeFieldRendererId();
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();

  auto frame1 = web::FakeWebFrame::Create("subframe1", false, GURL(kTestUrl));
  web::FakeWebFrame* frame1_ptr = frame1.get();
  AddWebFrame(std::move(frame1));

  auto frame2 = web::FakeWebFrame::Create("subframe2", false, GURL(kTestUrl));
  web::FakeWebFrame* frame2_ptr = frame2.get();
  AddWebFrame(std::move(frame2));

  OCMExpect([delegate_
      suggestionHelperShouldTriggerFormExtraction:helper_
                                          inFrame:main_frame_]);
  OCMExpect([delegate_ suggestionHelperShouldTriggerFormExtraction:helper_
                                                           inFrame:frame1_ptr]);
  OCMExpect([delegate_ suggestionHelperShouldTriggerFormExtraction:helper_
                                                           inFrame:frame2_ptr]);

  // First query for form in main frame.
  __block BOOL retrieved_suggestions1 = NO;
  __block BOOL completion1_called = NO;
  SuggestionsAvailableCompletion completion1 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions1 = suggestionsAvailable;
    completion1_called = YES;
  };
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form1_renderer_id, password1_renderer_id, NSFrameId(main_frame_));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:completion1];
  }

  // Second query for other form in main frame.
  __block BOOL retrieved_suggestions2 = NO;
  __block BOOL completion2_called = NO;
  SuggestionsAvailableCompletion completion2 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions2 = suggestionsAvailable;
    completion2_called = YES;
  };
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form1_renderer_id, password1_renderer_id, NSFrameId(main_frame_));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:completion2];
  }

  // Third query for form in subframe.
  __block BOOL retrieved_suggestions3 = NO;
  __block BOOL completion3_called = NO;
  SuggestionsAvailableCompletion completion3 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions3 = suggestionsAvailable;
    completion3_called = YES;
  };
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form1_renderer_id, password1_renderer_id, NSFrameId(frame1_ptr));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:completion3];
  }

  // Fourth query for form in other subframe.
  __block BOOL retrieved_suggestions4 = NO;
  __block BOOL completion4_called = NO;
  SuggestionsAvailableCompletion completion4 = ^(BOOL suggestionsAvailable) {
    retrieved_suggestions4 = suggestionsAvailable;
    completion4_called = YES;
  };
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form1_renderer_id, password1_renderer_id, NSFrameId(frame2_ptr));
    // Queue the third query to check if suggestions are available. At this
    // point there are 4 queries in the queue to be executed when processing
    // fill data.
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:completion4];
  }

  // The completions shouldn't be called yet as there is no fill data for the
  // queried forms. The helper triggers forms extraction in an attempt to get
  // fill data for the form.
  ASSERT_EQ(NO, completion1_called);
  ASSERT_EQ(NO, completion2_called);
  ASSERT_EQ(NO, completion3_called);
  ASSERT_EQ(NO, completion4_called);

  // Process fill data for the queried form in the main frame.
  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:main_frame_->GetFrameId()]);
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form1_renderer_id, username1_renderer_id, password1_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:main_frame_->GetFrameId()
                                 isMainFrame:main_frame_->IsMainFrame()
                           forSecurityOrigin:main_frame_->GetSecurityOrigin()];
  }
  // Queries for the forms in main frame should be completed after processing
  // the fill data for that frame.
  ASSERT_EQ(YES, completion1_called);
  ASSERT_EQ(YES, completion2_called);
  EXPECT_EQ(YES, retrieved_suggestions1);
  EXPECT_EQ(YES, retrieved_suggestions2);

  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:frame1_ptr->GetFrameId()]);
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form1_renderer_id, username1_renderer_id, password1_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame1_ptr->GetFrameId()
                                 isMainFrame:frame1_ptr->IsMainFrame()
                           forSecurityOrigin:frame1_ptr->GetSecurityOrigin()];
  }
  // Queries for the forms in first subframe should be completed after
  // processing the fill data for that frame.
  ASSERT_EQ(YES, completion3_called);
  EXPECT_EQ(YES, retrieved_suggestions3);

  OCMExpect([[delegate_ ignoringNonObjectArgs]
      attachListenersForBottomSheet:{}
                         forFrameId:frame2_ptr->GetFrameId()]);
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form1_renderer_id, username1_renderer_id, password1_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame2_ptr->GetFrameId()
                                 isMainFrame:frame2_ptr->IsMainFrame()
                           forSecurityOrigin:frame2_ptr->GetSecurityOrigin()];
  }
  // Queries for the forms in second subframe should be completed after
  // processing the fill data for that frame.
  ASSERT_EQ(YES, completion4_called);
  EXPECT_EQ(YES, retrieved_suggestions4);

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that the completed check queries are popped out of the queue. The
// test will crash with a CHECK failure if a query is run more than one time.
TEST_F(PasswordSuggestionHelperTest,
       CheckIfSuggestions_CompletedQueriesPoppedOut) {
  FormRendererId form_renderer_id = autofill::test::MakeFormRendererId();
  FieldRendererId username_renderer_id = autofill::test::MakeFieldRendererId();
  FieldRendererId password_renderer_id = autofill::test::MakeFieldRendererId();

  auto frame1 = web::FakeWebFrame::Create("subframe1", false, GURL(kTestUrl));
  web::FakeWebFrame* frame1_ptr = frame1.get();
  AddWebFrame(std::move(frame1));

  auto frame2 = web::FakeWebFrame::Create("subframe2", false, GURL(kTestUrl));
  web::FakeWebFrame* frame2_ptr = frame2.get();
  AddWebFrame(std::move(frame2));

  // First query for form in main frame.
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form_renderer_id, password_renderer_id, NSFrameId(main_frame_));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:^(BOOL suggestionsAvailable){
                              }];
  }

  // Second query for form in main frame.
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form_renderer_id, password_renderer_id, NSFrameId(main_frame_));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:^(BOOL suggestionsAvailable){
                              }];
  }

  // Third query for form in first subframe.
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form_renderer_id, password_renderer_id, NSFrameId(frame1_ptr));
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:^(BOOL suggestionsAvailable){
                              }];
  }

  // Fourth query for form in second subframe.
  {
    FormSuggestionProviderQuery* query = BuildPasswordQuery(
        form_renderer_id, password_renderer_id, NSFrameId(frame2_ptr));
    // Queue the third query to check if suggestions are available. At this
    // point there are 4 queries in the queue to be executed the next time fill
    // data is processed.
    [helper_ checkIfSuggestionsAvailableForForm:query
                              completionHandler:^(BOOL suggestionsAvailable){
                              }];
  }

  // Process fill data for the main frame.

  // Process the fill data a first time where all queries for main frame must
  // be popped out.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:main_frame_->GetFrameId()
                                 isMainFrame:main_frame_->IsMainFrame()
                           forSecurityOrigin:main_frame_->GetSecurityOrigin()];
  }
  // Process the fill data a second time to verify that all the queries for
  // the main frame were popped out of the queue in which case the query
  // only-once CHECK won't fail.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:main_frame_->GetFrameId()
                                 isMainFrame:main_frame_->IsMainFrame()
                           forSecurityOrigin:main_frame_->GetSecurityOrigin()];
  }

  // Process fill data for the first subframe.

  // Process the fill data a first time where all queries for the first
  // subframe must be popped out.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame1_ptr->GetFrameId()
                                 isMainFrame:frame1_ptr->IsMainFrame()
                           forSecurityOrigin:frame1_ptr->GetSecurityOrigin()];
  }
  // Process the fill data a second time to verify that all the queries for
  // the first subframe were popped out of the queue in which case the query
  // only-once CHECK won't fail.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame1_ptr->GetFrameId()
                                 isMainFrame:frame1_ptr->IsMainFrame()
                           forSecurityOrigin:frame1_ptr->GetSecurityOrigin()];
  }

  // Process fill data  for the second subframe.

  // Process the fill data a first time where all queries for the second
  // subframe must be popped out.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame2_ptr->GetFrameId()
                                 isMainFrame:frame2_ptr->IsMainFrame()
                           forSecurityOrigin:frame2_ptr->GetSecurityOrigin()];
  }
  // Process the fill data a second time to verify that all the queries for
  // the second subframe were popped out of the queue in which case the query
  // only-once CHECK won't fail.
  {
    PasswordFormFillData form_fill_data = CreatePasswordFillData(
        form_renderer_id, username_renderer_id, password_renderer_id);
    [helper_ processWithPasswordFormFillData:form_fill_data
                                  forFrameId:frame2_ptr->GetFrameId()
                                 isMainFrame:frame2_ptr->IsMainFrame()
                           forSecurityOrigin:frame2_ptr->GetSecurityOrigin()];
  }

  // Reaching this line means the no CHECK were triggered and that the queued
  // queries were correctly popped out.
}

// Tests retrieving suggestions on username field in form when available.
TEST_F(PasswordSuggestionHelperTest, RetrieveSuggestions_OnUsernameField) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = query.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();

  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  NSArray<FormSuggestion*>* suggestions =
      [helper_ retrieveSuggestionsWithForm:query];

  ASSERT_EQ(1ul, [suggestions count]);

  FormSuggestion* suggestionToEvaluate = suggestions.firstObject;

  EXPECT_NSEQ(SysUTF8ToNSString(kFillDataUsername), suggestionToEvaluate.value);
  EXPECT_FALSE(suggestionToEvaluate.metadata.is_single_username_form);
}

// Tests retrieving suggestions on password field in form when available.
TEST_F(PasswordSuggestionHelperTest, RetrieveSuggestions_OnPasswordField) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"password1", kObfuscatedFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = autofill::test::MakeFieldRendererId();
  FieldRendererId password1_renderer_id = query.fieldRendererID;

  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  NSArray<FormSuggestion*>* suggestions =
      [helper_ retrieveSuggestionsWithForm:query];

  ASSERT_EQ(1ul, [suggestions count]);

  FormSuggestion* suggestionToEvaluate = suggestions.firstObject;

  EXPECT_NSEQ(SysUTF8ToNSString(kFillDataUsername), suggestionToEvaluate.value);
  EXPECT_FALSE(suggestionToEvaluate.metadata.is_single_username_form);
}

// Tests retrieving suggestions for a single username form on the username
// field.
TEST_F(PasswordSuggestionHelperTest, RetrieveSuggestions_OnSingleUsernameForm) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = query.fieldRendererID;
  FieldRendererId password1_renderer_id = FieldRendererId();

  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  NSArray<FormSuggestion*>* suggestions =
      [helper_ retrieveSuggestionsWithForm:query];

  ASSERT_EQ(1ul, [suggestions count]);

  FormSuggestion* suggestionToEvaluate = suggestions.firstObject;

  EXPECT_NSEQ(SysUTF8ToNSString(kFillDataUsername), suggestionToEvaluate.value);
  EXPECT_TRUE(suggestionToEvaluate.metadata.is_single_username_form);
}

// Tests retrieving suggestions for form when there are no suggestions.
TEST_F(PasswordSuggestionHelperTest, RetrieveSuggestions_Empty) {
  FormSuggestionProviderQuery* form1_query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));

  // Create a 2nd form with the same username field id but a different form id.
  FormRendererId form2_renderer_id = autofill::test::MakeFormRendererId();
  FieldRendererId username1_renderer_id = form1_query.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();
  ASSERT_NE(form1_query.formRendererID, form2_renderer_id);

  // Process fill data for form2.
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form2_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  // Try to get suggestions for form1 which doesn't have fill data.
  NSArray<FormSuggestion*>* suggestions =
      [helper_ retrieveSuggestionsWithForm:form1_query];

  ASSERT_EQ(0ul, [suggestions count]);
}

// Tests getting password fill data when available.
TEST_F(PasswordSuggestionHelperTest, GetPasswordFillData) {
  FormSuggestionProviderQuery* query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = query.formRendererID;
  FieldRendererId username1_renderer_id = query.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();

  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  // Get suggestions first before getting the fill data for the selected
  // suggestion because this is a mandatory step.
  NSArray<FormSuggestion*>* suggestions =
      [helper_ retrieveSuggestionsWithForm:query];

  std::unique_ptr<password_manager::FillData> fill_data =
      [helper_ passwordFillDataForUsername:SysUTF8ToNSString(kFillDataUsername)
                                forFrameId:main_frame_->GetFrameId()];

  ASSERT_EQ(1ul, [suggestions count]);

  EXPECT_EQ(GURL(kTestUrl), (*fill_data).origin);
  EXPECT_EQ(form1_renderer_id, (*fill_data).form_id);
  EXPECT_EQ(username1_renderer_id, (*fill_data).username_element_id);
  EXPECT_EQ(UTF8ToUTF16(std::string("john.doe@gmail.com")),
            (*fill_data).username_value);
  EXPECT_EQ(password1_renderer_id, (*fill_data).password_element_id);
  EXPECT_EQ(UTF8ToUTF16(std::string("super!secret")),
            (*fill_data).password_value);

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that the helper is correctly reset.
TEST_F(PasswordSuggestionHelperTest, ResetForNewPage) {
  FormSuggestionProviderQuery* main_frame_query =
      BuildQuery(@"username1", kTextFieldType, NSFrameId(main_frame_));
  FormRendererId form1_renderer_id = main_frame_query.formRendererID;
  FieldRendererId username1_renderer_id = main_frame_query.fieldRendererID;
  FieldRendererId password1_renderer_id = autofill::test::MakeFieldRendererId();

  auto frame1 = web::FakeWebFrame::Create("subframe1", false, GURL(kTestUrl));
  web::FakeWebFrame* frame1_ptr = frame1.get();
  AddWebFrame(std::move(frame1));

  // Queue check query for subframe.
  __block BOOL completion_called = NO;
  SuggestionsAvailableCompletion completion = ^(BOOL suggestionsAvailable) {
    completion_called = YES;
  };
  {
    FormSuggestionProviderQuery* iframe_query =
        BuildQuery(@"password1", kObfuscatedFieldType, NSFrameId(frame1_ptr));
    [helper_ checkIfSuggestionsAvailableForForm:iframe_query
                              completionHandler:completion];
  }

  // Process fill data for main frame.
  PasswordFormFillData form_fill_data = CreatePasswordFillData(
      form1_renderer_id, username1_renderer_id, password1_renderer_id);
  [helper_ processWithPasswordFormFillData:form_fill_data
                                forFrameId:main_frame_->GetFrameId()
                               isMainFrame:main_frame_->IsMainFrame()
                         forSecurityOrigin:main_frame_->GetSecurityOrigin()];

  {
    // Get suggestions and fill data for main frame when there is still fill
    // data.
    NSArray<FormSuggestion*>* suggestions =
        [helper_ retrieveSuggestionsWithForm:main_frame_query];
    std::unique_ptr<password_manager::FillData> fill_data = [helper_
        passwordFillDataForUsername:SysUTF8ToNSString(kFillDataUsername)
                         forFrameId:main_frame_->GetFrameId()];

    // Check that there are suggestions for the main frame before the reset.
    ASSERT_EQ(1ul, [suggestions count]);
    ASSERT_TRUE(fill_data);
  }
  [helper_ resetForNewPage];

  {
    // Retry to get suggestions for the main frame which had processed fill data
    // before the reset.
    NSArray<FormSuggestion*>* suggestions =
        [helper_ retrieveSuggestionsWithForm:main_frame_query];
    // Check that there shouldn't be fill data anymore for the main frame.
    EXPECT_EQ(0ul, [suggestions count]);
  }

  // Process fill data after the reset where the query completion queue should
  // be empty.
  [helper_ processWithNoSavedCredentialsWithFrameId:frame1_ptr->GetFrameId()];
  EXPECT_EQ(NO, completion_called);

  EXPECT_OCMOCK_VERIFY(delegate_);
}
