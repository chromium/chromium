// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#import <string>

#import "base/apple/bundle_locations.h"
#import "base/json/json_writer.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/gtest_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "base/values.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/filling_product.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#import "components/autofill/core/browser/ui/suggestion.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/mock_password_autofill_agent_delegate.h"
#import "components/autofill/ios/browser/password_autofill_agent.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/prefs/pref_service.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_test.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/abseil-cpp/absl/types/variant.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/gfx/image/image_unittest_util.h"
#import "url/gurl.h"

using autofill::AutofillJavaScriptFeature;
using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FillingProduct;
using autofill::FormRendererId;
using autofill::SuggestionType;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

using autofill::AutofillDriverIOS;
using autofill::AutofillDriverIOSFactory;

// Returns the minimal FormData content for testing filling.
std::vector<autofill::FormFieldData::FillData>
MinimalFormFieldDataForFilling() {
  autofill::FormFieldData field;
  field.set_value(u"test-username");
  field.set_host_form_id(FormRendererId(1));
  field.set_renderer_id(FieldRendererId(2));
  field.set_is_autofilled(true);
  return {autofill::FormFieldData::FillData(std::move(field))};
}

// Returns a simple form suggestion that only consists of a `value` and a `type`
FormSuggestion* SimpleFormSuggestion(std::u16string value,
                                     autofill::SuggestionType type) {
  return [FormSuggestion suggestionWithValue:base::SysUTF16ToNSString(value)
                          displayDescription:@""
                                        icon:nil
                                        type:type
                           backendIdentifier:@""
                              requiresReauth:NO];
}

}  // namespace

@interface AutofillAgent (Testing)
- (void)updateFieldManagerWithFillingResults:(NSString*)jsonString
                                     inFrame:(web::WebFrame*)frame;
@end

// Test fixture for AutofillAgent testing.
class AutofillAgentTests : public web::WebTest {
 public:
  AutofillAgentTests() = default;

  AutofillAgentTests(const AutofillAgentTests&) = delete;
  AutofillAgentTests& operator=(const AutofillAgentTests&) = delete;

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    fake_web_frames_manager_->AddWebFrame(std::move(frame));
  }

  void RemoveWebFrame(const std::string& frame_id) {
    fake_web_frames_manager_->RemoveWebFrame(frame_id);
  }

  void SetUp() override {
    web::WebTest::SetUp();

    OverrideJavaScriptFeatures(
        {autofill::AutofillJavaScriptFeature::GetInstance(),
         autofill::FormHandlersJavaScriptFeature::GetInstance(),
         autofill::FormUtilJavaScriptFeature::GetInstance()});

    fake_web_state_.SetBrowserState(GetBrowserState());
    fake_web_state_.SetContentIsHTML(true);
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    web::ContentWorld content_world =
        AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    fake_web_state_.SetWebFramesManager(content_world,
                                        std::move(frames_manager));

    GURL url("https://example.com");
    fake_web_state_.SetCurrentURL(url);
    auto main_frame = web::FakeWebFrame::Create("frameID", true, url);
    main_frame->set_browser_state(GetBrowserState());
    fake_main_frame_ = main_frame.get();
    AddWebFrame(std::move(main_frame));

    autofill::PasswordAutofillAgent::CreateForWebState(&fake_web_state_,
                                                       &delegate_mock_);

    prefs_ = autofill::test::PrefServiceForTesting();
    autofill::prefs::SetAutofillProfileEnabled(prefs_.get(), true);
    autofill::prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), true);
    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:prefs_.get()
                                          webState:&fake_web_state_];
  }

  std::unique_ptr<web::FakeWebFrame> CreateMainWebFrame() {
    std::unique_ptr<web::FakeWebFrame> frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL());
    frame->set_browser_state(GetBrowserState());
    return frame;
  }

  std::unique_ptr<web::FakeWebFrame> CreateChildWebFrame() {
    std::unique_ptr<web::FakeWebFrame> frame =
        web::FakeWebFrame::CreateChildWebFrame(GURL());
    frame->set_browser_state(GetBrowserState());
    return frame;
  }

  // The prefs_ must outlive the fake_web_state_ and the autofill_agent_,
  // the latter of which can be de-allocated as part of de-allocating the
  // fake_web_state_.
  std::unique_ptr<PrefService> prefs_;
  // The client_ needs to outlive the fake_web_state_, which owns the
  // frames.
  autofill::TestAutofillClient client_;
  web::FakeWebState fake_web_state_;
  raw_ptr<web::FakeWebFrame> fake_main_frame_ = nullptr;
  raw_ptr<web::FakeWebFramesManager> fake_web_frames_manager_ = nullptr;
  AutofillAgent* autofill_agent_;
  autofill::MockPasswordAutofillAgentDelegate delegate_mock_;
};

// Tests that form's name and fields' identifiers, values, and whether they are
// autofilled are sent to the JS. Fields with empty values and those that are
// not autofilled are skipped. Tests logic based on renderer ids usage.
TEST_F(AutofillAgentTests,
       OnFormDataFilledTestWithFrameMessagingUsingRendererIDs) {
  std::string locale("en");
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              locale);

  std::vector<autofill::FormFieldData::FillData> fill_data;
  autofill::FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_label(u"Card number");
  field.set_name(u"number");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"number");
  field.set_value(u"number_value");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(2));
  fill_data.push_back(autofill::FormFieldData::FillData(field));
  field.set_label(u"Name on Card");
  field.set_name(u"name");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"name");
  field.set_value(u"name_value");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(3));
  fill_data.push_back(autofill::FormFieldData::FillData(field));
  field.set_label(u"Expiry Month");
  field.set_name(u"expiry_month");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"expiry_month");
  field.set_value(u"01");
  field.set_is_autofilled(false);
  field.set_renderer_id(FieldRendererId(4));
  fill_data.push_back(autofill::FormFieldData::FillData(field));
  field.set_label(u"Unknown field");
  field.set_name(u"unknown");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"unknown");
  field.set_value(u"");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(5));
  fill_data.push_back(autofill::FormFieldData::FillData(field));

  [autofill_agent_ fillData:fill_data
                    inFrame:fake_web_frames_manager_->GetMainWebFrame()];
  fake_web_state_.WasShown();

  EXPECT_EQ(u"__gCrWeb.autofill.fillForm({\"fields\":{\"2\":{\"hostFormId\":0,"
            u"\"section\":\"-default\",\"value\":\"number_value\"},\"3\":{"
            u"\"hostFormId\":0,\"section\":\"-default\","
            u"\"value\":\"name_value\"}}}, 0);",
            fake_main_frame_->GetLastJavaScriptCall());
}

// Tests that `fillSpecificFormField` in `autofill_agent_` dispatches the
// correct javascript call to the autofill controller.
TEST_F(AutofillAgentTests, FillSpecificFormField) {
  std::string locale("en");
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              locale);

  autofill::FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_label(u"Card number");
  field.set_name(u"number");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"number");
  field.set_value(u"number_value");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(2));

  [autofill_agent_
      fillSpecificFormField:field.renderer_id()
                  withValue:u"mattwashere"
                    inFrame:fake_web_frames_manager_->GetMainWebFrame()];
  fake_web_state_.WasShown();
  EXPECT_EQ(u"__gCrWeb.autofill.fillSpecificFormField({\"renderer_id\":"
            u"2,\"value\":\"mattwashere\"});",
            fake_main_frame_->GetLastJavaScriptCall());
}

// Test that the updates are applied when filling specific form field is done
// successfully.
TEST_F(AutofillAgentTests,
       FillSpecificFormField_UpdateWithResults_WhenSuccess) {
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              "en");

  std::vector<autofill::FormFieldData::FillData> fields =
      MinimalFormFieldDataForFilling();
  const std::u16string& field_value = fields[0].value;
  const FieldRendererId field_id = fields[0].renderer_id;

  // Set the result returned from filling.
  base::Value result(true);
  fake_main_frame_->AddJsResultForFunctionCall(
      &result, "autofill.fillSpecificFormField");

  EXPECT_CALL(delegate_mock_, DidFillField(fake_main_frame_.get(),
                                           std::optional<FormRendererId>(),
                                           field_id, field_value));

  // Declare the page as shown to allow filling.
  fake_web_state_.WasShown();

  // Fill form data.
  [autofill_agent_ fillSpecificFormField:field_id
                               withValue:field_value
                                 inFrame:fake_main_frame_];

  // Run queues to yield the filling results.
  web::test::WaitForBackgroundTasks();

  // Check that the field value update was propagated to the FieldDataManager of
  // the web frame.
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(fake_main_frame_);
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(field_id));
}

// Test that the updates aren't applied when filling specific form field has
// failed.
TEST_F(AutofillAgentTests,
       FillSpecificFormField_UpdateWithResults_WhenFailure) {
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              "en");

  std::vector<autofill::FormFieldData::FillData> fields =
      MinimalFormFieldDataForFilling();
  const std::u16string& field_value = fields[0].value;
  const FieldRendererId field_id = fields[0].renderer_id;

  // Set the result returned from filling.
  base::Value result(false);
  fake_main_frame_->AddJsResultForFunctionCall(
      &result, "autofill.fillSpecificFormField");

  EXPECT_CALL(delegate_mock_, DidFillField).Times(0);

  // Declare the page as shown to allow filling.
  fake_web_state_.WasShown();

  // Fill form data.
  [autofill_agent_ fillSpecificFormField:field_id
                               withValue:field_value
                                 inFrame:fake_main_frame_];

  // Run queues to yield the filling results.
  web::test::WaitForBackgroundTasks();

  // Check that the field value update was not propagated to the
  // FieldDataManager.
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(fake_main_frame_);
  EXPECT_FALSE(fieldDataManager->WasAutofilledOnUserTrigger(field_id));
}

// Tests that `ApplyFieldAction` in `AutofillDriverIOS` dispatches the
// correct javascript call to the autofill controller.
TEST_F(AutofillAgentTests, DriverFillSpecificFormField) {
  std::string locale("en");
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_,
                                              autofill_agent_, locale);

  autofill::FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_label(u"Card number");
  field.set_name(u"number");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"number");
  field.set_value(u"number_value");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(2));

  AutofillDriverIOS* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(
          &fake_web_state_, fake_web_frames_manager_->GetMainWebFrame());
  main_frame_driver->ApplyFieldAction(
      autofill::mojom::FieldActionType::kReplaceAll,
      autofill::mojom::ActionPersistence::kFill, field.global_id(),
      u"mattwashere");

  fake_web_state_.WasShown();
  EXPECT_EQ(u"__gCrWeb.autofill.fillSpecificFormField({\"renderer_id\":"
            u"2,\"value\":\"mattwashere\"});",
            fake_main_frame_->GetLastJavaScriptCall());
}

// Tests that `ApplyFieldAction` with `ActionPersistence::kPreview`in
// `AutofillDriverIOS` does not dispatch a JS call.
TEST_F(AutofillAgentTests, DriverPreviewSpecificFormField) {
  std::string locale("en");
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_,
                                              autofill_agent_, locale);

  autofill::FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_label(u"Card number");
  field.set_name(u"number");
  field.set_name_attribute(field.name());
  field.set_id_attribute(u"number");
  field.set_value(u"number_value");
  field.set_is_autofilled(true);
  field.set_renderer_id(FieldRendererId(2));

  AutofillDriverIOS* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(
          &fake_web_state_, fake_web_frames_manager_->GetMainWebFrame());
  // Preview is not currently supported; no JS should be run.
  main_frame_driver->ApplyFieldAction(
      autofill::mojom::FieldActionType::kReplaceAll,
      autofill::mojom::ActionPersistence::kPreview, field.global_id(),
      u"mattwashere");

  fake_web_state_.WasShown();
  EXPECT_EQ(u"", fake_main_frame_->GetLastJavaScriptCall());
}

// Tests that when a non user initiated form activity is registered the
// completion callback passed to the call to check if suggestions are available
// is invoked with no suggestions.
TEST_F(AutofillAgentTests,
       CheckIfSuggestionsAvailable_NonUserInitiatedActivity) {
  __block BOOL completion_handler_success = NO;
  __block BOOL completion_handler_called = NO;

  FormSuggestionProviderQuery* form_query =
      [[FormSuggestionProviderQuery alloc] initWithFormName:@"form"
                                             formRendererID:FormRendererId(1)
                                            fieldIdentifier:@"address"
                                            fieldRendererID:FieldRendererId(2)
                                                  fieldType:@"text"
                                                       type:@"focus"
                                                 typedValue:@""
                                                    frameID:@"frameID"];
  [autofill_agent_ checkIfSuggestionsAvailableForForm:form_query
                                       hasUserGesture:NO
                                             webState:&fake_web_state_
                                    completionHandler:^(BOOL success) {
                                      completion_handler_success = success;
                                      completion_handler_called = YES;
                                    }];
  fake_web_state_.WasShown();

  // Wait until the expected handler is called.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_called;
      }));
  EXPECT_FALSE(completion_handler_success);
}

// Tests that "Show credit cards from account" opt-in is shown.
TEST_F(AutofillAgentTests, onSuggestionsReady_ShowAccountCards) {
  __block NSArray<FormSuggestion*>* completion_handler_suggestions = nil;
  __block BOOL completion_handler_called = NO;

  autofill::MockAutofillSuggestionDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, OnSuggestionsShown);

  // Make the suggestions available to AutofillAgent.
  std::vector<autofill::Suggestion> autofillSuggestions;
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           SuggestionType::kShowAccountCards));
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];

  // Retrieves the suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_suggestions = [suggestions copy];
    completion_handler_called = YES;
  };
  FormSuggestionProviderQuery* form_query =
      [[FormSuggestionProviderQuery alloc] initWithFormName:@"form"
                                             formRendererID:FormRendererId(1)
                                            fieldIdentifier:@"address"
                                            fieldRendererID:FieldRendererId(2)
                                                  fieldType:@"text"
                                                       type:@"focus"
                                                 typedValue:@""
                                                    frameID:@"frameID"];
  [autofill_agent_ retrieveSuggestionsForForm:form_query
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  fake_web_state_.WasShown();

  // Wait until the expected handler is called.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_called;
      }));

  // "Show credit cards from account" should be the only suggestion.
  EXPECT_EQ(1U, completion_handler_suggestions.count);
  EXPECT_EQ(SuggestionType::kShowAccountCards,
            completion_handler_suggestions[0].type);
}

// Tests that virtual cards are being served as suggestions with the
// wanted string values of (main_text, ' ', minor_text) where the main_text
// is the 'Virtual card' string and the minor_text is the card name + last 4 or
// the card holder's name
TEST_F(AutofillAgentTests, showAutofillPopup_ShowVirtualCards) {
  __block NSUInteger suggestion_array_size = 0;
  __block FormSuggestion* virtual_card_suggestion = nil;
  __block FormSuggestion* credit_card_suggestion = nil;
  UIImage* visa_icon =
      ui::ResourceBundle::GetSharedInstance()
          .GetNativeImageNamed(autofill::CreditCard::IconResourceId("visaCC"))
          .ToUIImage();
  NSString* expiration_date_display_description = base::SysUTF8ToNSString(
      autofill::test::NextMonth() + "/" + autofill::test::NextYear().substr(2));
  // Mock different popup types.
  testing::NiceMock<autofill::MockAutofillSuggestionDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, GetMainFillingProduct)
      .WillOnce(testing::Return(FillingProduct::kCreditCard))
      .WillOnce(testing::Return(FillingProduct::kCreditCard));

  const std::string expiration_date_label = base::StrCat(
      {autofill::test::NextMonth(), "/", autofill::test::NextYear().substr(2)});

  // Initialize suggestion.
  std::vector<autofill::Suggestion> autofillSuggestions = {
      autofill::Suggestion("Virtual card", "Quicksilver ••1111",
                           expiration_date_label,
                           autofill::Suggestion::Icon::kCardVisa,
                           autofill::SuggestionType::kVirtualCreditCardEntry),
      autofill::Suggestion("Quicksilver ••1111", "", expiration_date_label,
                           autofill::Suggestion::Icon::kCardVisa,
                           autofill::SuggestionType::kCreditCardEntry),
  };

  // Completion handler to retrieve suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    suggestion_array_size = suggestions.count;
    virtual_card_suggestion = [FormSuggestion
                suggestionWithValue:[suggestions[0].value copy]
                         minorValue:[suggestions[0].minorValue copy]
                 displayDescription:[suggestions[0].displayDescription copy]
                               icon:[suggestions[0].icon copy]
                               type:suggestions[0].type
                  backendIdentifier:suggestions[0].backendIdentifier
        fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                     requiresReauth:suggestions[0].requiresReauth
         acceptanceA11yAnnouncement:[suggestions[0]
                                            .acceptanceA11yAnnouncement copy]];
    credit_card_suggestion = [FormSuggestion
                suggestionWithValue:[suggestions[1].value copy]
                         minorValue:[suggestions[1].minorValue copy]
                 displayDescription:[suggestions[1].displayDescription copy]
                               icon:[suggestions[1].icon copy]
                               type:suggestions[1].type
                  backendIdentifier:suggestions[1].backendIdentifier
        fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                     requiresReauth:suggestions[1].requiresReauth
         acceptanceA11yAnnouncement:[suggestions[1]
                                            .acceptanceA11yAnnouncement copy]];
  };

  // Make credit card suggestion.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];

  // Confirm both suggestions present
  ASSERT_EQ(2U, suggestion_array_size);

  // Confirm virtual card suggestion properties
  EXPECT_NSEQ(@"Virtual card", virtual_card_suggestion.value);
  EXPECT_NSEQ(@"Quicksilver ••1111", virtual_card_suggestion.minorValue);
  EXPECT_NSEQ(expiration_date_display_description,
              virtual_card_suggestion.displayDescription);
  EXPECT_TRUE(
      gfx::test::PlatformImagesEqual(virtual_card_suggestion.icon, visa_icon));
  EXPECT_EQ(autofill::SuggestionType::kVirtualCreditCardEntry,
            virtual_card_suggestion.type);
  EXPECT_NSEQ(@"", virtual_card_suggestion.backendIdentifier);
  EXPECT_EQ(false, virtual_card_suggestion.requiresReauth);
  EXPECT_NSEQ(nil, virtual_card_suggestion.acceptanceA11yAnnouncement);

  // Confirm credit card suggestion properties
  EXPECT_NSEQ(@"Quicksilver ••1111", credit_card_suggestion.value);
  EXPECT_NSEQ(nil, credit_card_suggestion.minorValue);
  EXPECT_NSEQ(expiration_date_display_description,
              credit_card_suggestion.displayDescription);
  EXPECT_TRUE(
      gfx::test::PlatformImagesEqual(credit_card_suggestion.icon, visa_icon));
  EXPECT_EQ(autofill::SuggestionType::kCreditCardEntry,
            credit_card_suggestion.type);
  EXPECT_NSEQ(@"", credit_card_suggestion.backendIdentifier);
  EXPECT_EQ(false, credit_card_suggestion.requiresReauth);
  EXPECT_NSEQ(nil, credit_card_suggestion.acceptanceA11yAnnouncement);
}

// Tests that only credit card suggestions would have icons.
TEST_F(AutofillAgentTests,
       showAutofillPopup_ShowIconForCreditCardSuggestionsOnly) {
  __block UIImage* completion_handler_icon = nil;

  // Mock different popup types.
  testing::NiceMock<autofill::MockAutofillSuggestionDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, GetMainFillingProduct)
      .WillOnce(testing::Return(FillingProduct::kCreditCard))
      .WillOnce(testing::Return(FillingProduct::kAddress))
      .WillOnce(testing::Return(FillingProduct::kNone));
  // Initialize suggestion.
  std::vector<autofill::Suggestion> autofillSuggestions = {
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kCardVisa,
                           autofill::SuggestionType::kCreditCardEntry),
      // This suggestion has a valid credit card icon, but the Suggestion type
      // (kShowAccountCards) is wrong.
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kCardVisa,
                           autofill::SuggestionType::kShowAccountCards),
  };
  // Completion handler to retrieve suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    ASSERT_EQ(2U, suggestions.count);
    completion_handler_icon = [suggestions[0].icon copy];
    // The non-credit card suggestion should never have an icon.
    EXPECT_EQ(nil, suggestions[1].icon);
  };

  // Make credit card suggestion.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_NE(nil, completion_handler_icon);
  // Make address suggestion.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_EQ(nil, completion_handler_icon);
  // Make unspecified suggestion.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_EQ(nil, completion_handler_icon);
}

// Tests that an empty network icon in a credit card suggestion will not cause
// any problems. Regression test for crbug.com/1446933
TEST_F(AutofillAgentTests, showAutofillPopup_EmptyIconInCreditCardSuggestion) {
  // Deliberately initialize this as non-nil, as we are expecting it to be set
  // to nil by the test.
  __block UIImage* completion_handler_icon = gfx::test::CreatePlatformImage();
  ASSERT_NE(nil, completion_handler_icon);

  testing::NiceMock<autofill::MockAutofillSuggestionDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, GetMainFillingProduct)
      .WillRepeatedly(testing::Return(FillingProduct::kCreditCard));

  std::vector<autofill::Suggestion> autofillSuggestions = {
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kCreditCardEntry)};

  // Completion handler to retrieve suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_icon = [suggestions[0].icon copy];
  };

  // Make credit card suggestion.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_EQ(nil, completion_handler_icon);
}

// Verify that plus address suggestions are handled appropriately in
// `showAutofillPopup`.
TEST_F(AutofillAgentTests, showAutofillPopup_PlusAddresses) {
  __block NSArray<FormSuggestion*>* completion_handler_suggestions = nil;
  __block BOOL completion_handler_called = NO;
  testing::NiceMock<autofill::MockAutofillSuggestionDelegate> mock_delegate;

  const std::string createSuggestionText = "create";
  const std::string fillExistingSuggestionText = "existing";
  std::vector<autofill::Suggestion> autofillSuggestions = {
      autofill::Suggestion(createSuggestionText, "",
                           autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kCreateNewPlusAddress),
      autofill::Suggestion(fillExistingSuggestionText, "",
                           autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kFillExistingPlusAddress)};

  // Completion handler to retrieve suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_suggestions = [suggestions copy];
    completion_handler_called = YES;
  };

  // Make plus address suggestions and note the conversion to `FormSuggestion`
  // objects.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];

  // Wait until the expected handler is called.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_called;
      }));

  // The plus address suggestions should be handled by the conversion to
  // `FormSuggestion` objects.
  EXPECT_EQ(2U, completion_handler_suggestions.count);
  EXPECT_EQ(SuggestionType::kCreateNewPlusAddress,
            completion_handler_suggestions[0].type);
  EXPECT_NSEQ(base::SysUTF8ToNSString(createSuggestionText),
              completion_handler_suggestions[0].value);
  EXPECT_EQ(autofill::SuggestionType::kFillExistingPlusAddress,
            completion_handler_suggestions[1].type);
  EXPECT_NSEQ(base::SysUTF8ToNSString(fillExistingSuggestionText),
              completion_handler_suggestions[1].value);
}

// Tests that for credit cards, a custom icon is preferred over the default
// icon.
TEST_F(AutofillAgentTests,
       showAutofillPopup_PreferCustomIconForCreditCardSuggestions) {
  autofill::Suggestion::Icon suggestion_network_icon =
      autofill::Suggestion::Icon::kCardVisa;
  UIImage* network_icon_image =
      ui::ResourceBundle::GetSharedInstance()
          .GetNativeImageNamed(
              autofill::CreditCard::IconResourceId(suggestion_network_icon))
          .ToUIImage();
  gfx::Image custom_icon = gfx::test::CreateImage(5, 5);

  testing::NiceMock<autofill::MockAutofillSuggestionDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, GetMainFillingProduct)
      .WillRepeatedly(testing::Return(FillingProduct::kCreditCard));

  // Completion handler to retrieve suggestions.
  __block UIImage* completion_handler_icon = nil;
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_icon = [suggestions[0].icon copy];
  };

  // Initialize suggestion, initially without a custom icon.
  std::vector<autofill::Suggestion> autofillSuggestions = {
      autofill::Suggestion("", "", suggestion_network_icon,
                           autofill::SuggestionType::kCreditCardEntry)};
  ASSERT_TRUE(
      absl::holds_alternative<gfx::Image>(autofillSuggestions[0].custom_icon));
  ASSERT_TRUE(
      absl::get<gfx::Image>(autofillSuggestions[0].custom_icon).IsEmpty());

  // When the custom icon is not present, the default icon should be used.
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_TRUE(gfx::test::PlatformImagesEqual(completion_handler_icon,
                                             network_icon_image));

  // Now set a custom icon, which should override the default.
  autofillSuggestions[0].custom_icon = custom_icon;
  [autofill_agent_ showAutofillPopup:autofillSuggestions
                  suggestionDelegate:mock_delegate.GetWeakPtr()];
  [autofill_agent_ retrieveSuggestionsForForm:nil
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  EXPECT_TRUE(gfx::test::PlatformImagesEqual(completion_handler_icon,
                                             custom_icon.ToUIImage()));
}

// Tests that when Autofill suggestions are made available to AutofillAgent
// "Clear Form" is moved to the start of the list and the order of other
// suggestions remains unchanged.
TEST_F(AutofillAgentTests, onSuggestionsReady_ClearForm) {
  __block NSArray<FormSuggestion*>* completion_handler_suggestions = nil;
  __block BOOL completion_handler_called = NO;

  // Make the suggestions available to AutofillAgent.
  std::vector<autofill::Suggestion> autofillSuggestions;
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kAddressEntry));
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kAddressEntry));
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           SuggestionType::kUndoOrClear));
  [autofill_agent_
       showAutofillPopup:autofillSuggestions
      suggestionDelegate:base::WeakPtr<autofill::AutofillSuggestionDelegate>()];

  // Retrieves the suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_suggestions = [suggestions copy];
    completion_handler_called = YES;
  };
  FormSuggestionProviderQuery* form_query =
      [[FormSuggestionProviderQuery alloc] initWithFormName:@"form"
                                             formRendererID:FormRendererId(1)
                                            fieldIdentifier:@"address"
                                            fieldRendererID:FieldRendererId(2)
                                                  fieldType:@"text"
                                                       type:@"focus"
                                                 typedValue:@""
                                                    frameID:@"frameID"];
  [autofill_agent_ retrieveSuggestionsForForm:form_query
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  fake_web_state_.WasShown();

  // Wait until the expected handler is called.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_called;
      }));

  // "Clear Form" should appear as the first suggestion. Otherwise, the order of
  // suggestions should not change.
  EXPECT_EQ(3U, completion_handler_suggestions.count);
  EXPECT_EQ(SuggestionType::kUndoOrClear,
            completion_handler_suggestions[0].type);
  EXPECT_EQ(autofill::SuggestionType::kAddressEntry,
            completion_handler_suggestions[1].type);
  EXPECT_EQ(autofill::SuggestionType::kAddressEntry,
            completion_handler_suggestions[2].type);
}

// Tests that when Autofill suggestions are made available to AutofillAgent
// GPay icon remains as the first suggestion.
TEST_F(AutofillAgentTests, onSuggestionsReady_ClearFormWithGPay) {
  __block NSArray<FormSuggestion*>* completion_handler_suggestions = nil;
  __block BOOL completion_handler_called = NO;

  // Make the suggestions available to AutofillAgent.
  std::vector<autofill::Suggestion> autofillSuggestions;
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kCreditCardEntry));
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           autofill::SuggestionType::kCreditCardEntry));
  autofillSuggestions.push_back(
      autofill::Suggestion("", "", autofill::Suggestion::Icon::kNoIcon,
                           SuggestionType::kUndoOrClear));
  [autofill_agent_
       showAutofillPopup:autofillSuggestions
      suggestionDelegate:base::WeakPtr<autofill::AutofillSuggestionDelegate>()];

  // Retrieves the suggestions.
  auto completionHandler = ^(NSArray<FormSuggestion*>* suggestions,
                             id<FormSuggestionProvider> delegate) {
    completion_handler_suggestions = [suggestions copy];
    completion_handler_called = YES;
  };
  FormSuggestionProviderQuery* form_query =
      [[FormSuggestionProviderQuery alloc] initWithFormName:@"form"
                                             formRendererID:FormRendererId(1)
                                            fieldIdentifier:@"address"
                                            fieldRendererID:FieldRendererId(2)
                                                  fieldType:@"text"
                                                       type:@"focus"
                                                 typedValue:@""
                                                    frameID:@"frameID"];
  [autofill_agent_ retrieveSuggestionsForForm:form_query
                                     webState:&fake_web_state_
                            completionHandler:completionHandler];
  fake_web_state_.WasShown();

  // Wait until the expected handler is called.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_called;
      }));

  EXPECT_EQ(3U, completion_handler_suggestions.count);
  EXPECT_EQ(SuggestionType::kUndoOrClear,
            completion_handler_suggestions[0].type);
  EXPECT_EQ(autofill::SuggestionType::kCreditCardEntry,
            completion_handler_suggestions[1].type);
  EXPECT_EQ(autofill::SuggestionType::kCreditCardEntry,
            completion_handler_suggestions[2].type);
}

// Test that every frames are processed whatever is the order of pageloading
// callbacks. The main frame should always be processed first.
class AutofillAgentTestFrameInitializationOrderFrames
    : public AutofillAgentTests {
 public:
  void SetUp() override {
    AutofillAgentTests::SetUp();
    RemoveWebFrame(fake_main_frame_->GetFrameId());
    ASSERT_FALSE(AutofillDriverIOSFactory::FromWebState(&fake_web_state_));
    AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                                "en");
  }
};

// Both frames available, then page loaded.
TEST_F(AutofillAgentTestFrameInitializationOrderFrames,
       BothFramesAvailableThenPageLoaded) {
  fake_web_state_.SetLoading(true);
  std::unique_ptr<web::FakeWebFrame> main_frame_unique = CreateMainWebFrame();
  web::FakeWebFrame* main_frame = main_frame_unique.get();
  AddWebFrame(std::move(main_frame_unique));
  AutofillDriverIOS* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, main_frame);
  EXPECT_TRUE(main_frame_driver->IsInAnyMainFrame());
  auto iframe_unique = CreateChildWebFrame();
  iframe_unique->set_call_java_script_function_callback(base::BindRepeating(^{
    EXPECT_TRUE(main_frame_driver->is_processed());
  }));
  web::FakeWebFrame* iframe = iframe_unique.get();
  AddWebFrame(std::move(iframe_unique));
  AutofillDriverIOS* iframe_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, iframe);
  EXPECT_FALSE(iframe_driver->IsInAnyMainFrame());
  EXPECT_FALSE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  fake_web_state_.SetLoading(false);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_TRUE(iframe_driver->is_processed());
  RemoveWebFrame(main_frame->GetFrameId());
  RemoveWebFrame(iframe->GetFrameId());
}

// Main frame available, then page loaded, then iframe available.
TEST_F(AutofillAgentTestFrameInitializationOrderFrames,
       MainFrameAvailableThenPageLoadedThenIframeAvailable) {
  std::unique_ptr<web::FakeWebFrame> main_frame_unique = CreateMainWebFrame();
  web::FakeWebFrame* main_frame = main_frame_unique.get();
  auto* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, main_frame);
  std::unique_ptr<web::FakeWebFrame> iframe_unique = CreateChildWebFrame();
  iframe_unique->set_call_java_script_function_callback(base::BindRepeating(^{
    EXPECT_TRUE(main_frame_driver->is_processed());
  }));
  web::FakeWebFrame* iframe = iframe_unique.get();
  auto* iframe_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, iframe);
  fake_web_state_.SetLoading(true);
  AddWebFrame(std::move(main_frame_unique));
  EXPECT_FALSE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  fake_web_state_.SetLoading(false);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  AddWebFrame(std::move(iframe_unique));
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_TRUE(iframe_driver->is_processed());
  RemoveWebFrame(main_frame->GetFrameId());
  RemoveWebFrame(iframe->GetFrameId());
}

// Page loaded, then main frame, then iframe.
TEST_F(AutofillAgentTestFrameInitializationOrderFrames,
       PageLoadedThenMainFrameThenIframe) {
  std::unique_ptr<web::FakeWebFrame> main_frame_unique = CreateMainWebFrame();
  web::FakeWebFrame* main_frame = main_frame_unique.get();
  auto* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, main_frame);
  std::unique_ptr<web::FakeWebFrame> iframe_unique = CreateChildWebFrame();
  iframe_unique->set_call_java_script_function_callback(base::BindRepeating(^{
    EXPECT_TRUE(main_frame_driver->is_processed());
  }));
  web::FakeWebFrame* iframe = iframe_unique.get();
  auto* iframe_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, iframe);
  fake_web_state_.SetLoading(true);
  fake_web_state_.SetLoading(false);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  AddWebFrame(std::move(main_frame_unique));
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  AddWebFrame(std::move(iframe_unique));
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_TRUE(iframe_driver->is_processed());
  RemoveWebFrame(main_frame->GetFrameId());
  RemoveWebFrame(iframe->GetFrameId());
}

// Page loaded, then iframe, then main frame.
TEST_F(AutofillAgentTestFrameInitializationOrderFrames,
       PageLoadedThenIframeThenMainFrame) {
  std::unique_ptr<web::FakeWebFrame> main_frame_unique = CreateMainWebFrame();
  web::FakeWebFrame* main_frame = main_frame_unique.get();
  auto* main_frame_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, main_frame);
  std::unique_ptr<web::FakeWebFrame> iframe_unique = CreateChildWebFrame();
  iframe_unique->set_call_java_script_function_callback(base::BindRepeating(^{
    EXPECT_TRUE(main_frame_driver->is_processed());
  }));
  web::FakeWebFrame* iframe = iframe_unique.get();
  auto* iframe_driver =
      AutofillDriverIOS::FromWebStateAndWebFrame(&fake_web_state_, iframe);
  fake_web_state_.SetLoading(true);
  fake_web_state_.SetLoading(false);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  AddWebFrame(std::move(iframe_unique));
  EXPECT_FALSE(main_frame_driver->is_processed());
  EXPECT_FALSE(iframe_driver->is_processed());
  AddWebFrame(std::move(main_frame_unique));
  EXPECT_TRUE(main_frame_driver->is_processed());
  EXPECT_TRUE(iframe_driver->is_processed());
  RemoveWebFrame(main_frame->GetFrameId());
  RemoveWebFrame(iframe->GetFrameId());
}

TEST_F(AutofillAgentTests, FillData_UpdateWithResults) {
  auto test_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  std::string locale("en");
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              locale);

  std::vector<autofill::FormFieldData::FillData> fields =
      MinimalFormFieldDataForFilling();
  const std::u16string& field_value = fields[0].value;
  const FormRendererId form_id = fields[0].host_form_id;
  const FieldRendererId field_id = fields[0].renderer_id;

  // Set the result returned from filling.
  std::string serializedResult;
  ASSERT_TRUE(base::JSONWriter::Write(
      base::Value::Dict().Set(base::NumberToString(field_id.value()),
                              base::UTF16ToUTF8(field_value)),
      &serializedResult));
  base::Value result(serializedResult);
  fake_main_frame_->AddJsResultForFunctionCall(&result, "autofill.fillForm");

  EXPECT_CALL(delegate_mock_,
              DidFillField(fake_main_frame_.get(),
                           std::make_optional<FormRendererId>(form_id),
                           field_id, field_value));

  // Declare the page as shown to allow filling.
  fake_web_state_.WasShown();

  // Fill form data.
  [autofill_agent_ fillData:fields inFrame:fake_main_frame_];

  // Run queues to yield the filling results.
  web::test::WaitForBackgroundTasks();

  // Check that the field value update was propagated to the FieldDataManager of
  // the web frame.
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(fake_main_frame_);
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(field_id));

  // Check recorded UKM.
  auto entries = test_recorder->GetEntriesByName(
      ukm::builders::Autofill_FormFillSuccessIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder->ExpectEntryMetric(entries[0], "FormFillSuccess", true);
}

// Tests that if there is an unknown field id in the results, the agent isn't
// notified.
TEST_F(AutofillAgentTests, FillData_UnknowFieldIdInResults) {
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              "en");

  std::vector<autofill::FormFieldData::FillData> fields =
      MinimalFormFieldDataForFilling();
  const FieldRendererId unknown_field_id = FieldRendererId(101);

  // Set the result returned from filling.
  std::string serializedResult;
  ASSERT_TRUE(base::JSONWriter::Write(
      base::Value::Dict().Set(base::NumberToString(unknown_field_id.value()),
                              base::UTF16ToUTF8(fields[0].value)),
      &serializedResult));
  base::Value result(serializedResult);
  fake_main_frame_->AddJsResultForFunctionCall(&result, "autofill.fillForm");

  EXPECT_CALL(delegate_mock_, DidFillField).Times(0);

  // Declare the page as shown to allow filling.
  fake_web_state_.WasShown();

  // Fill form data.
  [autofill_agent_ fillData:fields inFrame:fake_main_frame_];

  // Run queues to yield the filling results.
  web::test::WaitForBackgroundTasks();
}

// Tests selecting an autocomplete suggestion.
TEST_F(AutofillAgentTests, DidSelectSuggestion_AutocompleteEntry) {
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              /*locale=*/"en");

  FormRendererId form_id(1);
  FieldRendererId field1_id(2);
  const std::u16string field1_value = u"test-value";

  // Set the result returned from filling as a success.
  base::Value result(true);
  fake_main_frame_->AddJsResultForFunctionCall(&result,
                                               "autofill.fillActiveFormField");

  // Declare the page as shown to allow field filling.
  fake_web_state_.WasShown();

  // Select suggestion to trigger field filling.
  __block BOOL completion_handler_called = NO;
  FormSuggestion* form_suggestion = SimpleFormSuggestion(
      field1_value, autofill::SuggestionType::kAutocompleteEntry);
  [autofill_agent_ didSelectSuggestion:form_suggestion
                               atIndex:0
                                  form:@"single-username-form"
                        formRendererID:form_id
                       fieldIdentifier:@"username-field-1"
                       fieldRendererID:field1_id
                               frameID:base::SysUTF8ToNSString(
                                           fake_main_frame_->GetFrameId())
                     completionHandler:^() {
                       completion_handler_called = YES;
                     }];

  EXPECT_CALL(delegate_mock_,
              DidFillField(fake_main_frame_.get(),
                           std::make_optional<FormRendererId>(form_id),
                           field1_id, field1_value));

  // Run queues to yield the field filling results from the JS call.
  web::test::WaitForBackgroundTasks();

  // Check that the field value update was propagated to the FieldDataManager of
  // the web frame.
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(fake_main_frame_);
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(field1_id));

  // Check that the completion handler was called after handling the results
  // from the JS call.
  EXPECT_TRUE(completion_handler_called);
}

TEST_F(AutofillAgentTests, DidSelectSuggestion_ClearFormEntry) {
  AutofillDriverIOSFactory::CreateForWebState(&fake_web_state_, &client_, nil,
                                              /*locale=*/"en");

  FormRendererId form_id(1);
  FieldRendererId field1_id(2);
  FieldRendererId field2_id(3);

  // Set the result returned from filling.
  std::string serializedResult;
  ASSERT_TRUE(base::JSONWriter::Write(
      base::Value::List()
          .Append(base::Value(base::NumberToString(field1_id.value())))
          .Append(base::Value(base::NumberToString(field2_id.value()))),
      &serializedResult));
  base::Value result(serializedResult);
  fake_main_frame_->AddJsResultForFunctionCall(
      &result, "autofill.clearAutofilledFields");

  // Declare the page as shown to allow field filling.
  fake_web_state_.WasShown();

  // Select suggestion to trigger field filling.
  __block BOOL completion_handler_called = NO;
  FormSuggestion* form_suggestion =
      SimpleFormSuggestion(u"", autofill::SuggestionType::kUndoOrClear);
  [autofill_agent_ didSelectSuggestion:form_suggestion
                               atIndex:0
                                  form:@"single-username-form"
                        formRendererID:form_id
                       fieldIdentifier:@"username-field-1"
                       fieldRendererID:field1_id
                               frameID:base::SysUTF8ToNSString(
                                           fake_main_frame_->GetFrameId())
                     completionHandler:^() {
                       completion_handler_called = YES;
                     }];

  EXPECT_CALL(delegate_mock_,
              DidFillField(fake_main_frame_.get(),
                           std::make_optional<FormRendererId>(form_id),
                           field1_id, ::testing::IsEmpty()));
  EXPECT_CALL(delegate_mock_,
              DidFillField(fake_main_frame_.get(),
                           std::make_optional<FormRendererId>(form_id),
                           field2_id, ::testing::IsEmpty()));

  // Run queues to yield the field filling results from the JS call.
  web::test::WaitForBackgroundTasks();

  // Check that the cleared field IDs aren't labeled as filled.
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(fake_main_frame_);
  EXPECT_FALSE(fieldDataManager->WasAutofilledOnUserTrigger(field1_id));
  EXPECT_FALSE(fieldDataManager->WasAutofilledOnUserTrigger(field2_id));

  // Check that the completion handler was called after handling the results
  // from the JS call.
  EXPECT_TRUE(completion_handler_called);
}
