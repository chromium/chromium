// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#import <stddef.h>

#import <string>

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/password_manager/core/browser/mock_password_manager.h"
#import "components/password_manager/core/browser/password_manager_driver.h"
#import "components/password_manager/core/browser/password_manager_interface.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/password_manager/ios/test_helpers.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

NS_ASSUME_NONNULL_BEGIN

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::FillData;
using test_helpers::SetPasswordFormFillData;
using test_helpers::SetFillData;
using test_helpers::SetFormData;

namespace {

// A FakeWebState that returns nullopt as the last trusted committed URL.
class FakeWebStateWithoutTrustedCommittedUrl : public web::FakeWebState {
 public:
  ~FakeWebStateWithoutTrustedCommittedUrl() override = default;

  // WebState implementation.
  std::optional<GURL> GetLastCommittedURLIfTrusted() const override {
    return std::nullopt;
  }
};

class PasswordFormHelperTest : public AutofillTestWithWebState {
 public:
  PasswordFormHelperTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {autofill::FormHandlersJavaScriptFeature::GetInstance(),
         autofill::FormUtilJavaScriptFeature::GetInstance(),
         password_manager::PasswordManagerJavaScriptFeature::GetInstance()});
  }

  PasswordFormHelperTest(const PasswordFormHelperTest&) = delete;
  PasswordFormHelperTest& operator=(const PasswordFormHelperTest&) = delete;

  ~PasswordFormHelperTest() override = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();

    IOSPasswordManagerDriverFactory::CreateForWebState(
        web_state(), OCMStrictClassMock([SharedPasswordController class]),
        &password_manager_);

    helper_ = [[PasswordFormHelper alloc] initWithWebState:web_state()];
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override {
    WaitForBackgroundTasks();
    helper_ = nil;
    web::WebTestWithWebState::TearDown();
  }

  web::WebFrame* GetMainFrame() {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    return feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  }

  // Sets up unique form ids and returns true if successful.
  bool SetUpUniqueIDs() {
    __block web::WebFrame* main_frame = nullptr;
    bool success =
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
          main_frame = GetMainFrame();
          return main_frame != nullptr;
        });
    if (!success) {
      return false;
    }
    DCHECK(main_frame);

    // Run password forms search to set up unique IDs.
    __block bool complete = false;
    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->FindPasswordFormsInFrame(main_frame,
                                   base::BindOnce(^(NSString* forms) {
                                     complete = true;
                                   }));

    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return complete;
    });
  }

  // Returns a valid form submitted message body.
  std::unique_ptr<base::Value> ValidFormSubmittedMessageBody(
      std::string frame_id) {
    return std::make_unique<base::Value>(
        base::Value::Dict()
            .Set("name", "test_form")
            .Set("origin", BaseUrl())
            .Set("fields", base::Value::List().Append(
                               base::Value::Dict()
                                   .Set("name", "test_field")
                                   .Set("form_control_type", "password")))
            .Set("host_frame", frame_id));
  }

  // Returns a script message that can represent a form submission.
  web::ScriptMessage ScriptMessageForSubmit(std::unique_ptr<base::Value> body) {
    return web::ScriptMessage(std::move(body),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt);
  }

 protected:
  // PasswordFormHelper for testing.
  PasswordFormHelper* helper_;
  password_manager::MockPasswordManager password_manager_;
};

struct FindPasswordFormTestData {
  // HTML String of the form.
  NSString* html_string;
  // True if expected to find the form.
  const bool expected_form_found;
  // Expected number of fields in found form.
  const size_t expected_number_of_fields;
  // Expected form name.
  const char* expected_form_name;
};

// Check that HTML forms are converted correctly into PasswordForms.
TEST_F(PasswordFormHelperTest, FindPasswordFormsInView) {
  // clang-format off
  const FindPasswordFormTestData test_data[] = {
    // Normal form: a username and a password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user0'>"
      "<input type='password' name='pass0'>"
      "</form>",
      true, 2, "form1"
    },
    // User name is captured as an email address (HTML5).
    {
      @"<form name='form1'>"
      "<input type='email' name='email1'>"
      "<input type='password' name='pass1'>"
      "</form>",
      true, 2, "form1"
    },
    // No form found.
    {
      @"<div>",
      false, 0, nullptr
    },
    // Disabled username element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user2' disabled='disabled'>"
      "<input type='password' name='pass2'>"
      "</form>",
      true, 2, "form1"
    },
    // No password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user3'>"
      "</form>",
      false, 0, nullptr
    },
    // No <form> tag.
    {
      @"<input type='email' name='email1'>"
      "<input type='password' name='pass1'>",
      true, 2, ""
    },
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for html_string="
                 << base::SysNSStringToUTF8(data.html_string));
    LoadHtml(data.html_string);
    __block std::vector<FormData> forms;
    __block BOOL block_was_called = NO;
    [helper_ findPasswordFormsInFrame:GetMainFrame()
                    completionHandler:^(const std::vector<FormData>& result) {
                      block_was_called = YES;
                      forms = result;
                    }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(data.expected_number_of_fields, forms[0].fields().size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(forms[0].name()));
    } else {
      ASSERT_TRUE(forms.empty());
    }
  }
}

// A script that resets all text fields, including those in iframes.
static NSString* kClearInputFieldsScript =
    @"function clearInputFields(win) {"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    inputs[i].value = '';"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    clearInputFields(frames[i]);"
     "  }"
     "}"
     "clearInputFields(window);";

// A script that runs after autofilling forms.  It returns ids and values of all
// non-empty fields, including those in iframes.
static NSString* kInputFieldValueVerificationScript =
    @"function findAllInputsInFrame(win, prefix) {"
     "  var result = '';"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    var input = inputs[i];"
     "    if (input.value) {"
     "      result += prefix + input.id + '=' + input.value + ';';"
     "    }"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    result += findAllInputsInFrame("
     "        frames[i], prefix + frames[i].name +'.');"
     "  }"
     "  return result;"
     "};"
     "function findAllInputs(win) {"
     "  return findAllInputsInFrame(win, '');"
     "};"
     "findAllInputs(window);";

// Tests that filling password forms with fill data works correctly.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData_Success) {
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::HistogramTester histogram_tester;
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());
  const std::string base_url = BaseUrl();
  FormRendererId form_id(1);
  FieldRendererId username_field_id(2);
  const std::u16string username_value = u"john.doe@gmail.com";
  FieldRendererId password_field_id(3);
  const std::u16string password_value = u"super!secret";
  FillData fill_data;
  SetFillData(base_url, form_id.value(), username_field_id.value(),
              base::UTF16ToUTF8(username_value).c_str(),
              password_field_id.value(),
              base::UTF16ToUTF8(password_value).c_str(), &fill_data);

  web::WebFrame* frame = GetMainFrame();

  // Expect calls to the PasswordManager to update its state from the filled
  // inputs.
  IOSPasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state(),
                                                               frame);
  auto* field_data_manager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);
  EXPECT_CALL(
      password_manager_,
      UpdateStateOnUserInput(driver, ::testing::Ref(*field_data_manager),
                             std::make_optional<FormRendererId>(form_id),
                             username_field_id, username_value));
  EXPECT_CALL(
      password_manager_,
      UpdateStateOnUserInput(driver, ::testing::Ref(*field_data_manager),
                             std::make_optional<FormRendererId>(form_id),
                             password_field_id, password_value));

  __block bool called = false;
  __block BOOL succeeded = false;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:frame
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL success) {
                        called = true;
                        succeeded = success;
                      }];

  // Wait on the JS call to be completed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));

  // Verify that the completion callback is called with success as a result.
  EXPECT_TRUE(succeeded);

  // Verify that the username and password inputs were filled with their
  // respective value.
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=john.doe@gmail.com;p1=super!secret;", result);

  // Check that username and password fields were updated as filled in the
  // FieldDataManager.
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(GetMainFrame());
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(username_field_id));
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(password_field_id));

  // Verify that the fill operation was recorded as a success.
  histogram_tester.ExpectUniqueSample("PasswordManager.FillingSuccessIOS", true,
                                      1);

  // Check recorded UKM.
  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::PasswordManager_PasswordFillingIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder.ExpectEntryMetric(entries[0], "FillingSuccess", true);
}

// Tests that filling password form data can succeeds while not filling
// anything.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData_Success_NoFill) {
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::HistogramTester histogram_tester;
  // Set the username field as disabled and set a value for the password so it
  // is possible to have a succesful fill operation that returns success but
  // that doesn't fill anything in reality.
  LoadHtml(@"<form><input id='u1' type='text' name='un1' value='test-username' "
           @"disabled>"
            "<input id='p1' type='password' name='pw1' "
            "value='test-password'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());

  // Don't expect calls to the PasswordManager to update its state when failure
  // to fill.
  EXPECT_CALL(password_manager_, UpdateStateOnUserInput).Times(0);

  const std::string base_url = BaseUrl();
  FieldRendererId username_field_id(2);
  FieldRendererId password_field_id(3);
  FillData fill_data;
  // Set fill data with a password that is the same as the one that is already
  // in the password field.
  SetFillData(base_url, 1, username_field_id.value(), "test-username",
              password_field_id.value(), "test-password", &fill_data);

  __block bool called = false;
  __block BOOL succeeded = false;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL success) {
                        called = true;
                        succeeded = success;
                      }];

  // Wait on the JS call to be completed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));

  // Verify that the completion callback is called with success as the result.
  EXPECT_TRUE(succeeded);

  // Verify that the username and password inputs still hold their value.
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=test-username;p1=test-password;", result);

  // Check that username and password fields were still updated even if they
  // were not filled. This odd behavior is kept to not skew metrics downstream
  // (e.g. PasswordManager.FillingAssistance).
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(GetMainFrame());
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(username_field_id));
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(password_field_id));

  // Verify that the fill operation was recorded as a success.
  histogram_tester.ExpectUniqueSample("PasswordManager.FillingSuccessIOS", true,
                                      1);

  // Check recorded UKM.
  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::PasswordManager_PasswordFillingIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder.ExpectEntryMetric(entries[0], "FillingSuccess", true);
}

// Tests that failure in filling password forms with fill data is correctly
// handled.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData_Failure) {
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::HistogramTester histogram_tester;

  LoadHtml(@"<form><input id='p1' type='password' name='pw1'></form>");
  web::WebFrame* frame = GetMainFrame();

  ASSERT_TRUE(SetUpUniqueIDs());

  // Don't expect calls to the PasswordManager to update its state when failure
  // to fill.
  EXPECT_CALL(password_manager_, UpdateStateOnUserInput).Times(0);

  const std::string base_url = BaseUrl();
  FieldRendererId username_field_id(0);
  // The password renderer id does not exist, that's why the filling will fail
  FieldRendererId password_field_id(404);
  FillData fill_data;
  SetFillData(base_url, 1, username_field_id.value(), "",
              password_field_id.value(), "super!secret", &fill_data);

  __block bool called = false;
  __block BOOL succeeded = false;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:frame
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL success) {
                        called = true;
                        succeeded = success;
                      }];

  // Wait on the JS call to be completed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));

  // Verify that the completion callback is called with failure.
  EXPECT_FALSE(succeeded);

  // Check that username and password fields were NOT updated as filled in the
  // FieldDataManager.
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);
  EXPECT_FALSE(fieldDataManager->WasAutofilledOnUserTrigger(username_field_id));
  EXPECT_FALSE(fieldDataManager->WasAutofilledOnUserTrigger(password_field_id));

  // Verify that the fill operation was recorded as a failure.
  histogram_tester.ExpectUniqueSample("PasswordManager.FillingSuccessIOS",
                                      false, 1);
  // Check recorded UKM.
  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::PasswordManager_PasswordFillingIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder.ExpectEntryMetric(entries[0], "FillingSuccess", false);
}

// Tests that a form with username typed by user is not refilled when
// the user selects a filling suggestion on password field.
TEST_F(PasswordFormHelperTest,
       FillPasswordIntoForm_UserTypedUsername_FillFromPassword) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());

  FormRendererId form_id(1);
  FieldRendererId username_field_id(2);
  const std::u16string username_value = u"john.doe@gmail.com";
  FieldRendererId password_field_id(3);
  const std::u16string password_value = u"store!pw";

  web::WebFrame* frame = GetMainFrame();

  // Type on username field.
  ExecuteJavaScript(
      @"document.getElementById('u1').value = 'typed@typed.com';");
  [helper_ updateFieldDataOnUserInput:username_field_id
                              inFrame:GetMainFrame()
                           inputValue:@"typed@typed.com"];

  // Try to autofill the form.
  FillData fill_data;
  SetFillData(BaseUrl(), form_id.value(), username_field_id.value(),
              base::UTF16ToUTF8(username_value).c_str(),
              password_field_id.value(),
              base::UTF16ToUTF8(password_value).c_str(), &fill_data);

  IOSPasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(web_state(),
                                                               frame);
  auto* field_data_manager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);
  // Don't expect to update the state for the username field because it was
  // skipped.
  EXPECT_CALL(
      password_manager_,
      UpdateStateOnUserInput(driver, ::testing::Ref(*field_data_manager),
                             std::make_optional<FormRendererId>(form_id),
                             username_field_id, username_value))
      .Times(0);
  // Expect a state update on the password field.
  EXPECT_CALL(
      password_manager_,
      UpdateStateOnUserInput(driver, ::testing::Ref(*field_data_manager),
                             std::make_optional<FormRendererId>(form_id),
                             password_field_id, password_value));

  __block bool called = NO;
  __block bool succeeded = NO;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:password_field_id
                      completionHandler:^(BOOL success) {
                        called = YES;
                        succeeded = success;
                      }];

  // Wait on the JS call to be completed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));

  // Verify that the completion callback is called with success as the result.
  EXPECT_TRUE(succeeded);

  // Check that the password field was updated as filled in the
  // FieldDataManager but not the username as it wasn't filled.
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(GetMainFrame());
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(username_field_id));
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(password_field_id));

  // Verify that the password was filled.
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=typed@typed.com;p1=store!pw;", result);
}

// Tests that a form with username typed by user is overriden by fill when
// the user selects a filling suggestion on usernmae field.
TEST_F(PasswordFormHelperTest,
       FillPasswordIntoForm_UserTypedUsername_FillFromUsername) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");
  ASSERT_TRUE(SetUpUniqueIDs());

  const std::string base_url = BaseUrl();
  FieldRendererId username_field_id(2);
  FieldRendererId password_field_id(3);

  // Type on username field.
  ExecuteJavaScript(
      @"document.getElementById('u1').value = 'typed@typed.com';");
  [helper_ updateFieldDataOnUserInput:username_field_id
                              inFrame:GetMainFrame()
                           inputValue:@"typed@typed.com"];

  FillData fill_data;
  SetFillData(base_url, 1, username_field_id.value(), "john.doe@gmail.com",
              password_field_id.value(), "super!secret", &fill_data);
  __block bool called = false;
  __block BOOL succeeded = false;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL success) {
                        called = true;
                        succeeded = success;
                      }];

  // Wait on the JS call to be completed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));

  // Verify that the completion callback is called with success as a result.
  EXPECT_TRUE(succeeded);

  // Verify that the username and password inputs were filled with their
  // respective value.
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=john.doe@gmail.com;p1=super!secret;", result);

  // Check that username and password fields were updated as filled in the
  // FieldDataManager.
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(GetMainFrame());
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(username_field_id));
  EXPECT_TRUE(fieldDataManager->WasAutofilledOnUserTrigger(password_field_id));
}

//

// Tests that filling is considered as a failure if the returned result blob
// from the js call is missing a field or is of the wrong type. Tests all
// possibilities in the same test to avoid over boilerplating.
TEST_F(PasswordFormHelperTest, FillUsernameAndPassword_MissingFillResultField) {
  const std::string base_url = BaseUrl();
  web::FakeWebState fake_web_state;
  auto* feature =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance();
  web::ContentWorld content_world = feature->GetSupportedContentWorld();

  // Add feature to support the filling of password form data.
  web::test::OverrideJavaScriptFeatures(GetBrowserState(), {feature});

  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto* web_frames_manager_ptr = web_frames_manager.get();
  fake_web_state.SetWebFramesManager(content_world,
                                     std::move(web_frames_manager));
  fake_web_state.SetBrowserState(GetBrowserState());
  fake_web_state.SetContentIsHTML(true);
  fake_web_state.SetCurrentURL(GURL(base_url));

  std::unique_ptr<web::FakeWebFrame> main_frame =
      web::FakeWebFrame::Create("frameID", true, GURL(base_url));
  main_frame->set_browser_state(GetBrowserState());
  auto* main_frame_ptr = main_frame.get();
  web_frames_manager_ptr->AddWebFrame(std::move(main_frame));

  IOSPasswordManagerDriverFactory::CreateForWebState(
      &fake_web_state, OCMStrictClassMock([SharedPasswordController class]),
      &password_manager_);
  // Don't expect calls to the PasswordManager to update its state when failure
  // to fill.
  EXPECT_CALL(password_manager_, UpdateStateOnUserInput).Times(0);

  FieldRendererId username_field_id(2);
  FieldRendererId password_field_id(3);
  FillData fill_data;
  SetFillData(base_url, 1, username_field_id.value(), "test-username",
              password_field_id.value(), "super!secret", &fill_data);

  PasswordFormHelper* helper =
      [[PasswordFormHelper alloc] initWithWebState:&fake_web_state];

  // Test the missing did_fill_username field.
  {
    auto result = base::Value(base::Value::Dict()
                                  .Set("did_fill_password", base::Value(true))
                                  .Set("did_attempt_fill", base::Value(true)));
    main_frame_ptr->AddJsResultForFunctionCall(&result,
                                               "passwords.fillPasswordForm");
    __block bool called = false;
    __block BOOL succeeded = false;
    [helper fillPasswordFormWithFillData:fill_data
                                 inFrame:main_frame_ptr
                        triggeredOnField:username_field_id
                       completionHandler:^(BOOL success) {
                         called = true;
                         succeeded = success;
                       }];
    WaitForBackgroundTasks();
    ASSERT_TRUE(called);
    EXPECT_FALSE(succeeded);
  }

  // Test the missing did_fill_password field.
  {
    auto result = base::Value(base::Value::Dict()
                                  .Set("did_fill_username", base::Value(true))
                                  .Set("did_attempt_fill", base::Value(true)));
    main_frame_ptr->AddJsResultForFunctionCall(&result,
                                               "passwords.fillPasswordForm");
    __block bool called = false;
    __block BOOL succeeded = false;
    [helper fillPasswordFormWithFillData:fill_data
                                 inFrame:main_frame_ptr
                        triggeredOnField:username_field_id
                       completionHandler:^(BOOL success) {
                         called = true;
                         succeeded = success;
                       }];
    WaitForBackgroundTasks();
    ASSERT_TRUE(called);
    EXPECT_FALSE(succeeded);
  }

  // Test the missing did_attempt_fill field.
  {
    auto result = base::Value(base::Value::Dict()
                                  .Set("did_fill_username", base::Value(true))
                                  .Set("did_fill_password", base::Value(true)));
    main_frame_ptr->AddJsResultForFunctionCall(&result,
                                               "passwords.fillPasswordForm");
    __block bool called = false;
    __block BOOL succeeded = false;
    [helper fillPasswordFormWithFillData:fill_data
                                 inFrame:main_frame_ptr
                        triggeredOnField:username_field_id
                       completionHandler:^(BOOL success) {
                         called = true;
                         succeeded = success;
                       }];
    WaitForBackgroundTasks();
    ASSERT_TRUE(called);
    EXPECT_FALSE(succeeded);
  }

  // Test the wrong type of returned result.
  {
    base::Value result("string-result");
    main_frame_ptr->AddJsResultForFunctionCall(&result,
                                               "passwords.fillPasswordForm");
    __block bool called = false;
    __block BOOL succeeded = false;
    [helper fillPasswordFormWithFillData:fill_data
                                 inFrame:main_frame_ptr
                        triggeredOnField:username_field_id
                       completionHandler:^(BOOL success) {
                         called = true;
                         succeeded = success;
                       }];
    WaitForBackgroundTasks();
    ASSERT_TRUE(called);
    EXPECT_FALSE(succeeded);
  }

  // Test a nullptr result.
  {
    main_frame_ptr->AddJsResultForFunctionCall(nullptr,
                                               "passwords.fillPasswordForm");
    __block bool called = false;
    __block BOOL succeeded = false;
    [helper fillPasswordFormWithFillData:fill_data
                                 inFrame:main_frame_ptr
                        triggeredOnField:username_field_id
                       completionHandler:^(BOOL success) {
                         called = true;
                         succeeded = success;
                       }];
    WaitForBackgroundTasks();
    ASSERT_TRUE(called);
    EXPECT_FALSE(succeeded);
  }
}

// Tests that extractPasswordFormData extracts wanted form on page with mutiple
// forms.
TEST_F(PasswordFormHelperTest, ExtractPasswordFormData) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>"
            "<form><input id='u2' type='text' name='un2'>"
            "<input id='p2' type='password' name='pw2'></form>"
            "<form><input id='u3' type='text' name='un3'>"
            "<input id='p3' type='password' name='pw3'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());

  __block int call_counter = 0;
  __block int success_counter = 0;
  __block FormData result = FormData();
  [helper_ extractPasswordFormData:FormRendererId(1)
                           inFrame:GetMainFrame()
                 completionHandler:^(BOOL complete, const FormData& form) {
                   ++call_counter;
                   if (complete) {
                     ++success_counter;
                     result = form;
                   }
                 }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  EXPECT_EQ(1, success_counter);
  EXPECT_EQ(result.renderer_id(), FormRendererId(1));

  call_counter = 0;
  success_counter = 0;
  result = FormData();

  [helper_ extractPasswordFormData:FormRendererId(404)
                           inFrame:GetMainFrame()
                 completionHandler:^(BOOL complete, const FormData& form) {
                   ++call_counter;
                   if (complete) {
                     ++success_counter;
                     result = form;
                   }
                 }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  EXPECT_EQ(0, success_counter);
}

// Tests that the form submit message is fully handled when in the correct
// format and with the minimal viable content.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage) {
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  OCMExpect([[delegate ignoringNonObjectArgs] formHelper:helper_
                                           didSubmitForm:FormData()
                                                 inFrame:GetMainFrame()]);
  helper_.delegate = delegate;

  LoadHtml(@"<p>");

  // Set a message with the minimal viable body to succeed in form data
  // extraction.
  web::ScriptMessage submit_message = ScriptMessageForSubmit(
      ValidFormSubmittedMessageBody(GetMainFrame()->GetFrameId()));

  HandleSubmittedFormStatus status =
      [helper_ handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kHandled, status);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the form submit message isn't handled when the message isn't in
// the correct format.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage_InvalidFormat) {
  // Set the delegate mock in a way that the test will crash if there is any
  // delegate call to handle the message.
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  helper_.delegate = delegate;

  LoadHtml(@"<p>");

  // Set the message value content as a string which is an invalid format
  // because a dictionary is expected.
  auto invalid_body =
      std::make_unique<base::Value>(base::Value("invalid_because_expect_dict"));

  web::ScriptMessage submit_message =
      ScriptMessageForSubmit(std::move(invalid_body));

  HandleSubmittedFormStatus status =
      [helper_ handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kRejectedMessageBodyNotADict, status);

  // Verify that the delegate is never called because the message isn't handled
  // because of the early return.
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the form submit message isn't handled when there is no trusted URL
// loaded in the webstate.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage_NoTrustedUrl) {
  FakeWebStateWithoutTrustedCommittedUrl web_state;
  PasswordFormHelper* helper =
      [[PasswordFormHelper alloc] initWithWebState:&web_state];

  // Set the delegate mock in a way that the test will crash if there is any
  // delegate call to handle the message.
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  helper.delegate = delegate;

  // Set a dummy message.
  web::ScriptMessage submit_message =
      ScriptMessageForSubmit(std::make_unique<base::Value>("whatever"));

  HandleSubmittedFormStatus status =
      [helper handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kRejectedNoTrustedUrl, status);

  // Verify that the delegate is never called because the message isn't handled
  // because of the early return.
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the form submit message isn't handled when there is no webstate.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage_NoWebState) {
  // Set the delegate mock in a way that the test will crash if there is any
  // delegate call to handle the message.
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  helper_.delegate = delegate;

  // Set a dummy message.
  web::ScriptMessage submit_message =
      ScriptMessageForSubmit(std::make_unique<base::Value>("whatever"));

  // Destroying the webstate will nullify the webstate pointer in the helper.
  DestroyWebState();

  HandleSubmittedFormStatus status =
      [helper_ handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kRejectedNoWebState, status);

  // Verify that the delegate is never called because the message isn't handled
  // because of the early return.
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the form submit message isn't handled when there is no frame
// matching the provided frame id.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage_NoFormMatchingId) {
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  helper_.delegate = delegate;

  LoadHtml(@"<p>");

  // Set a message with an nonexisting frame id.
  web::ScriptMessage submit_message = ScriptMessageForSubmit(
      ValidFormSubmittedMessageBody("nonexisting_frame_id"));

  HandleSubmittedFormStatus status =
      [helper_ handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kRejectedNoFrameMatchingId, status);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the form submit message isn't handled when form data can't be
// extracted from the message's body.
TEST_F(PasswordFormHelperTest, HandleFormSubmittedMessage_CantExtractFormData) {
  id delegate = OCMStrictProtocolMock(@protocol(PasswordFormHelperDelegate));
  helper_.delegate = delegate;

  LoadHtml(@"<p>");

  auto incomplete_message_body = std::make_unique<base::Value>(
      base::Value::Dict().Set("host_frame", GetMainFrame()->GetFrameId()));

  // Set a message with an incomplete body that misses the required keys to be
  // parsed to form data.
  web::ScriptMessage submit_message =
      ScriptMessageForSubmit(std::move(incomplete_message_body));

  HandleSubmittedFormStatus status =
      [helper_ handleFormSubmittedMessage:submit_message];

  EXPECT_EQ(HandleSubmittedFormStatus::kRejectedCantExtractFormData, status);

  EXPECT_OCMOCK_VERIFY(delegate);
}

}  // namespace

NS_ASSUME_NONNULL_END
