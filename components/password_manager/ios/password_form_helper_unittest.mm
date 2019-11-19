// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/js_password_manager.h"
#import "components/password_manager/ios/password_form_helper.h"
#include "components/password_manager/ios/test_helpers.h"
#include "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NS_ASSUME_NONNULL_BEGIN

using autofill::FormData;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::FillData;
using test_helpers::SetPasswordFormFillData;
using test_helpers::SetFillData;

@interface PasswordFormHelper (Testing)

// Provides access to the method below for testing with mocks.
- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const PasswordForm& form))completionHandler;

// Provides access to replace |jsPasswordManager| with Mock one for test.
- (void)setJsPasswordManager:(JsPasswordManager*)jsPasswordManager;

@end

// Mocks JsPasswordManager to simluate javascript execution failure.
@interface MockJsPasswordManager : JsPasswordManager

// Designated initializer.
- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// For the first |targetFailureCount| calls to
// |fillPasswordForm:withUserName:password:completionHandler:|, skips the
// invocation of the real JavaScript manager, giving the effect that password
// form fill failed. As soon as |_fillPasswordFormFailureCountRemaining| reaches
// zero, stop mocking and let the original JavaScript manager execute.
- (void)setFillPasswordFormTargetFailureCount:(NSUInteger)targetFailureCount;

@end

@implementation MockJsPasswordManager {
  NSUInteger _fillPasswordFormFailureCountRemaining;
}

- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  return [super initWithReceiver:receiver];
}

- (void)setFillPasswordFormTargetFailureCount:(NSUInteger)targetFailureCount {
  _fillPasswordFormFailureCountRemaining = targetFailureCount;
}

- (void)fillPasswordForm:(NSString*)JSONString
            withUsername:(NSString*)username
                password:(NSString*)password
       completionHandler:(void (^)(BOOL))completionHandler {
  if (_fillPasswordFormFailureCountRemaining > 0) {
    --_fillPasswordFormFailureCountRemaining;
    if (completionHandler) {
      completionHandler(NO);
    }
    return;
  }
  [super fillPasswordForm:JSONString
             withUsername:username
                 password:password
        completionHandler:completionHandler];
}

@end

namespace {
// Returns a string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  EXPECT_NE(nil, script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  EXPECT_NE(nil, path);
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  EXPECT_EQ(nil, error);
  EXPECT_NE(nil, content);
  return content;
}

class TestWebClientWithScript : public web::TestWebClient {
 public:
  NSString* GetDocumentStartScriptForMainFrame(
      web::BrowserState* browser_state) const override {
    return GetPageScript(@"test_bundle");
  }
};

class PasswordFormHelperTest : public web::WebTestWithWebState {
 public:
  PasswordFormHelperTest()
      : web::WebTestWithWebState(std::make_unique<TestWebClientWithScript>()) {}

  ~PasswordFormHelperTest() override = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
    helper_ =
        [[PasswordFormHelper alloc] initWithWebState:web_state() delegate:nil];
  }

  void TearDown() override {
    WaitForBackgroundTasks();
    helper_ = nil;
    web::WebTestWithWebState::TearDown();
  }

 protected:
  // Returns an identifier for the |form_index|th form in the page.
  std::string GetFormId(int form_index) {
    NSString* kGetFormIdScript =
        @"__gCrWeb.form.getFormIdentifier("
         "    document.querySelectorAll('form')[%d]);";
    return base::SysNSStringToUTF8(ExecuteJavaScript(
        [NSString stringWithFormat:kGetFormIdScript, form_index]));
  }

  // PasswordFormHelper for testing.
  PasswordFormHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormHelperTest);
};

struct GetSubmittedPasswordFormTestData {
  // HTML String of the form.
  NSString* html_string;
  // Javascript to submit the form.
  NSString* java_script;
  // 0 based index of the form on the page to submit.
  const int index_of_the_form_to_submit;
  // Expected number of fields in found form.
  const size_t expected_number_of_fields;
  // Expected form name.
  const char* expected_form_name;
};

// Check that HTML forms are captured and converted correctly into
// PasswordForms on submission.
TEST_F(PasswordFormHelperTest, GetSubmittedPasswordForm) {
  // clang-format off
  const GetSubmittedPasswordFormTestData test_data[] = {
    // Two forms with no explicit names.
    {
      @"<form action='javascript:;'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='password' name='pass1' value='pw1'>"
      "</form>"
      "<form action='javascript:;'>"
      "<input type='text' name='user2' value='user2'>"
      "<input type='password' name='pass2' value='pw2'>"
      "<input type='submit' id='s2'>"
      "</form>",
      @"document.getElementById('s2').click()",
      1, 2, "gChrome~form~1"
    },
    // Two forms with explicit names.
    {
      @"<form name='test2a' action='javascript:;'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='password' name='pass1' value='pw1'>"
      "<input type='submit' id='s1'>"
      "</form>"
      "<form name='test2b' action='javascript:;' value='user2'>"
      "<input type='text' name='user2'>"
      "<input type='password' name='pass2' value='pw2'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, 2, "test2a"
    },
    // Not-password form.
    {
      @"<form action='javascript:;' name='form1'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='text' name='not_pass1' value='text1'>"
      "<input type='submit' id='s1'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, 2, "form1"
    },
    // Form with quotes in the form and field names.
    {
      @"<form name=\"foo'\" action='javascript:;'>"
      "<input type='text' name=\"user1'\" value='user1'>"
      "<input type='password' id='s1' name=\"pass1'\" value='pw2'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, 2, "foo'"
    },
  };
  // clang-format on

  for (const GetSubmittedPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string
                                    << " and java_script=" << data.java_script
                                    << " and index_of_the_form_to_submit="
                                    << data.index_of_the_form_to_submit);
    LoadHtml(data.html_string);
    ExecuteJavaScript(data.java_script);
    __block BOOL block_was_called = NO;
    id completion_handler = ^(BOOL found, const FormData& form) {
      block_was_called = YES;
      EXPECT_EQ(data.expected_number_of_fields, form.fields.size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(form.name));
    };
    [helper_
        extractSubmittedPasswordForm:GetFormId(data.index_of_the_form_to_submit)
                   completionHandler:completion_handler];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
  }
}

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
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for html_string="
                 << base::SysNSStringToUTF8(data.html_string));
    LoadHtml(data.html_string);
    __block std::vector<FormData> forms;
    __block BOOL block_was_called = NO;
    [helper_ findPasswordFormsWithCompletionHandler:^(
                 const std::vector<FormData>& result) {
      block_was_called = YES;
      forms = result;
    }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(data.expected_number_of_fields, forms[0].fields.size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(forms[0].name));
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

// Test HTML page.  It contains several password forms.  Tests autofill
// them and verify that the right ones are autofilled.
static NSString* kHtmlWithMultiplePasswordForms =
    @""
     // Basic form.
     "<form>"
     "<input id='un0' type='text' name='u0'>"
     "<input id='pw0' type='password' name='p0'>"
     "</form>"
     // Form with action in the same origin.
     "<form action='?query=yes#reference'>"
     "<input id='un1' type='text' name='u1'>"
     "<input id='pw1' type='password' name='p1'>"
     "</form>"
     // Form with action in other origin.
     "<form action='http://some_other_action'>"
     "<input id='un2' type='text' name='u2'>"
     "<input id='pw2' type='password' name='p2'>"
     "</form>"
     // Form with two exactly same password fields.
     "<form>"
     "<input id='un3' type='text' name='u3'>"
     "<input id='pw3' type='password' name='p3'>"
     "<input id='pw3' type='password' name='p3'>"
     "</form>"
     // Forms with same names but different ids (1 of 2).
     "<form>"
     "<input id='un4' type='text' name='u4'>"
     "<input id='pw4' type='password' name='p4'>"
     "</form>"
     // Forms with same names but different ids (2 of 2).
     "<form>"
     "<input id='un5' type='text' name='u4'>"
     "<input id='pw5' type='password' name='p4'>"
     "</form>"
     // Basic form, but with quotes in the names and IDs.
     "<form name=\"f6'\">"
     "<input id=\"un6'\" type='text' name=\"u6'\">"
     "<input id=\"pw6'\" type='password' name=\"p6'\">"
     "</form>"
     // Test forms inside iframes.
     "<iframe id='pf' name='pf'></iframe>"
     "<iframe id='npf' name='npf'></iframe>"
     "<script>"
     "  var doc = frames['pf'].document.open();"
     // Add a form inside iframe. It should also be matched and autofilled.
     // Note: The id and name fields are deliberately set as same as those of
     // some other fields outside of the frames. The algorithm should be
     // able to handle this conflict.
     "  doc.write('<form><input id=\\'un4\\' type=\\'text\\' name=\\'u4\\'>');"
     "  doc.write('<input id=\\'pw4\\' type=\\'password\\' name=\\'p4\\'>');"
     "  doc.write('</form>');"
     // Add a non-password form inside iframe. It should not be matched.
     // Note: Same as above, the type mismatch of id and name as well as
     // the conflict with existing fields are deliberately arranged.
     "  var doc = frames['npf'].document.open();"
     "  doc.write('<form><input id=\\'un4\\' type=\\'text\\' name=\\'u4\\'>');"
     "  doc.write('<input id=\\'pw4\\' type=\\'text\\' name=\\'p4\\'>');"
     "  doc.write('</form>');"
     "  doc.close();"
     "</script>"
     // Fields inside this form don't have name.
     "<form>"
     "<input id='un9' type='text'>"
     "<input id='pw9' type='password'>"
     "</form>"
     // Fields in this form is attached by form's id.
     "<form id='form10'></form>"
     "<input id='un10' type='text' form='form10'>"
     "<input id='pw10' type='password' form='form10'>";

struct FillPasswordFormTestData {
  // Origin of the form data.
  const std::string origin;
  // Action of the form data.
  const std::string action;
  // Name/id of the user name field in the form data.
  const char* username_field;
  // Value of the user name field in the form data.
  const char* username_value;
  // Name/id of the password field in the form data.
  const char* password_field;
  // Value of the password field in the form data.
  const char* password_value;
  // True if the match should be found.
  const BOOL should_succeed;
  // Expected result generated by |kInputFieldValueVerificationScript|.
  NSString* expected_result;
};

// Tests that filling password forms works correctly.
TEST_F(PasswordFormHelperTest, FillPasswordForm) {
  LoadHtml(kHtmlWithMultiplePasswordForms);

  const std::string base_url = BaseUrl();
  // clang-format off
  const FillPasswordFormTestData test_data[] = {
    // Basic test: one-to-one match on the first password form.
    {
      base_url,
      base_url,
      "un0",
      "test_user",
      "pw0",
      "test_password",
      YES,
      @"un0=test_user;pw0=test_password;"
    },
    // Multiple forms match (including one in iframe): they should all be
    // autofilled.
    {
      base_url,
      base_url,
      "un4",
      "test_user",
      "pw4",
      "test_password",
      YES,
      @"un4=test_user;pw4=test_password;pf.un4=test_user;pf.pw4=test_password;"
    },
    // The form matches despite a different action: the only difference
    // is a query and reference.
    {
      base_url,
      base_url,
      "un1",
      "test_user",
      "pw1",
      "test_password",
      YES,
      @"un1=test_user;pw1=test_password;"
    },
    // No match because of a different origin.
    {
      "http://someotherfakedomain.com",
      base_url,
      "un0",
      "test_user",
      "pw0",
      "test_password",
      NO,
      @""
    },
    // No match because of a different action.
    {
      base_url,
      "http://someotherfakedomain.com",
      "un0",
      "test_user",
      "pw0",
      "test_password",
      NO,
      @""
    },
    // No match because some inputs are not in the form.
    {
      base_url,
      base_url,
      "un0",
      "test_user",
      "pw1",
      "test_password",
      NO,
      @""
    },
    // There are inputs with duplicate names in the form, the first of them is
    // filled.
    {
      base_url,
      base_url,
      "un3",
      "test_user",
      "pw3",
      "test_password",
      YES,
      @"un3=test_user;pw3=test_password;"
    },
    // Basic test, but with quotes in the names and IDs.
    {
      base_url,
      base_url,
      "un6'",
      "test_user",
      "pw6'",
      "test_password",
      YES,
      @"un6'=test_user;pw6'=test_password;"
    },
    // Fields don't have name attributes so id attribute is used for fields
    // identification.
    {
      base_url,
      base_url,
      "un9",
      "test_user",
      "pw9",
      "test_password",
      YES,
      @"un9=test_user;pw9=test_password;"
    },
    // Fields in this form is attached by form's id.
    {
      base_url,
      base_url,
      "un10",
      "test_user",
      "pw10",
      "test_password",
      YES,
      @"un10=test_user;pw10=test_password;"
    },
  };
  // clang-format on

  for (const FillPasswordFormTestData& data : test_data) {
    ExecuteJavaScript(kClearInputFieldsScript);

    PasswordFormFillData form_data;
    SetPasswordFormFillData(data.origin, data.action, data.username_field,
                            data.username_value, data.password_field,
                            data.password_value, nullptr, nullptr, false,
                            &form_data);

    __block BOOL block_was_called = NO;
    [helper_ fillPasswordForm:form_data
            completionHandler:^(BOOL success) {
              block_was_called = YES;
              EXPECT_EQ(data.should_succeed, success);
            }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));

    id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
    EXPECT_NSEQ(data.expected_result, result);
  }
}

// Tests that filling password forms with fill data works correctly.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData) {
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>");
  const std::string base_url = BaseUrl();
  FillData fill_data;
  SetFillData(base_url, base_url, "u1", "john.doe@gmail.com", "p1",
              "super!secret", &fill_data);

  __block int call_counter = 0;
  [helper_ fillPasswordFormWithFillData:fill_data
                      completionHandler:^(BOOL complete) {
                        ++call_counter;
                        EXPECT_TRUE(complete);
                      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=john.doe@gmail.com;p1=super!secret;", result);
}

// Tests that a form is found and the found form is filled in with the given
// username and password.
TEST_F(PasswordFormHelperTest, FindAndFillOnePasswordForm) {
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>");
  __block int call_counter = 0;
  __block int success_counter = 0;
  [helper_ findAndFillPasswordFormsWithUserName:@"john.doe@gmail.com"
                                       password:@"super!secret"
                              completionHandler:^(BOOL complete) {
                                ++call_counter;
                                if (complete) {
                                  ++success_counter;
                                }
                              }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  EXPECT_EQ(1, success_counter);
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=john.doe@gmail.com;p1=super!secret;", result);
}

// Tests that multiple forms on the same page are found and filled.
// This test includes an mock injected failure on form filling to verify
// that completion handler is called with the proper values.
TEST_F(PasswordFormHelperTest, FindAndFillMultiplePasswordForms) {
  // Fails the first call to fill password form.
  MockJsPasswordManager* mockJsPasswordManager = [[MockJsPasswordManager alloc]
      initWithReceiver:web_state()->GetJSInjectionReceiver()];
  [mockJsPasswordManager setFillPasswordFormTargetFailureCount:1];
  [helper_ setJsPasswordManager:mockJsPasswordManager];
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>"
       "<form><input id='u2' type='text' name='un2'>"
       "<input id='p2' type='password' name='pw2'></form>"
       "<form><input id='u3' type='text' name='un3'>"
       "<input id='p3' type='password' name='pw3'></form>");
  __block int call_counter = 0;
  __block int success_counter = 0;
  [helper_ findAndFillPasswordFormsWithUserName:@"john.doe@gmail.com"
                                       password:@"super!secret"
                              completionHandler:^(BOOL complete) {
                                ++call_counter;
                                if (complete) {
                                  ++success_counter;
                                }
                              }];
  // There should be 3 password forms and only 2 successfully filled forms.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 3;
  }));
  EXPECT_EQ(2, success_counter);
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(
      @"u2=john.doe@gmail.com;p2=super!secret;"
       "u3=john.doe@gmail.com;p3=super!secret;",
      result);
}

// Tests that extractPasswordFormData extracts wanted form on page with mutiple
// forms.
TEST_F(PasswordFormHelperTest, ExtractPasswordFormData) {
  MockJsPasswordManager* mockJsPasswordManager = [[MockJsPasswordManager alloc]
      initWithReceiver:web_state()->GetJSInjectionReceiver()];
  [helper_ setJsPasswordManager:mockJsPasswordManager];
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>"
            "<form><input id='u2' type='text' name='un2'>"
            "<input id='p2' type='password' name='pw2'></form>"
            "<form><input id='u3' type='text' name='un3'>"
            "<input id='p3' type='password' name='pw3'></form>");
  __block int call_counter = 0;
  __block int success_counter = 0;
  __block FormData result = FormData();
  [helper_ extractPasswordFormData:base::SysUTF8ToNSString(GetFormId(1))
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
  EXPECT_EQ(result.name, base::ASCIIToUTF16(GetFormId(1)));

  call_counter = 0;
  success_counter = 0;
  result = FormData();

  [helper_ extractPasswordFormData:@"unknown"
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

}  // namespace

NS_ASSUME_NONNULL_END
