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
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/test_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    UniqueIDDataTabHelper::CreateForWebState(web_state());
    helper_ = [[PasswordFormHelper alloc] initWithWebState:web_state()];
  }

  void TearDown() override {
    WaitForBackgroundTasks();
    helper_ = nil;
    web::WebTestWithWebState::TearDown();
  }

 protected:
  // PasswordFormHelper for testing.
  PasswordFormHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormHelperTest);
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
    [helper_ findPasswordFormsWithCompletionHandler:^(
                 const std::vector<FormData>& result, uint32_t maxID) {
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

// Tests that filling password forms with fill data works correctly.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData) {
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>");
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(0);");
  // Run password forms search to set up unique IDs.
  EXPECT_TRUE(ExecuteJavaScript(@"__gCrWeb.passwords.findPasswordForms();"));
  const std::string base_url = BaseUrl();
  FillData fill_data;
  SetFillData(base_url, 0, 1, "john.doe@gmail.com", 2, "super!secret",
              &fill_data);

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
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(0);");
  // Run password forms search to set up unique IDs.
  EXPECT_TRUE(ExecuteJavaScript(@"__gCrWeb.passwords.findPasswordForms();"));

  PasswordFormFillData form_data;
  SetPasswordFormFillData(BaseUrl(), "gChrome~form~0", 0, "u1", 1,
                          "john.doe@gmail.com", "p1", 2, "super!secret",
                          nullptr, nullptr, false, &form_data);

  __block int call_counter = 0;
  __block int success_counter = 0;
  [helper_ fillPasswordForm:form_data
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

// Tests that extractPasswordFormData extracts wanted form on page with mutiple
// forms.
TEST_F(PasswordFormHelperTest, ExtractPasswordFormData) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>"
            "<form><input id='u2' type='text' name='un2'>"
            "<input id='p2' type='password' name='pw2'></form>"
            "<form><input id='u3' type='text' name='un3'>"
            "<input id='p3' type='password' name='pw3'></form>");
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(0);");
  // Run password forms search to set up unique IDs.
  EXPECT_TRUE(ExecuteJavaScript(@"__gCrWeb.passwords.findPasswordForms();"));
  __block int call_counter = 0;
  __block int success_counter = 0;
  __block FormData result = FormData();
  [helper_ extractPasswordFormData:FormRendererId(0)
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
  EXPECT_EQ(result.unique_renderer_id, FormRendererId(0));

  call_counter = 0;
  success_counter = 0;
  result = FormData();

  [helper_ extractPasswordFormData:FormRendererId(404)
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

// Tests that a form with credentials fillied on user trigger
// is not autorefilled.
TEST_F(PasswordFormHelperTest, RefillFormFilledOnUserTrigger) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(0);");
  // Run password forms search to set up unique IDs.
  EXPECT_TRUE(ExecuteJavaScript(@"__gCrWeb.passwords.findPasswordForms();"));

  // Fill the form on user trigger.
  const std::string base_url = BaseUrl();
  FillData fill_data;
  SetFillData(base_url, 0, 1, "john.doe@gmail.com", 2, "super!secret",
              &fill_data);
  __block int call_counter = 0;
  [helper_ fillPasswordFormWithFillData:fill_data
                      completionHandler:^(BOOL complete) {
                        ++call_counter;
                        EXPECT_TRUE(complete);
                      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));

  // Try to autofill the form.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(BaseUrl(), "", 0, "", 1, "someacc@store.com", "", 2,
                          "store!pw", "", "", NO, &form_data);

  __block bool called = NO;
  __block bool success = NO;
  [helper_ fillPasswordForm:form_data
          completionHandler:^(BOOL res) {
            called = YES;
            success = res;
          }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));
  EXPECT_EQ(success, NO);
}

// Tests that a form with credentials typed by user
// is not autorefilled.
TEST_F(PasswordFormHelperTest, RefillFormWithUserTypedInput) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");
  ExecuteJavaScript(@"__gCrWeb.fill.setUpForUniqueIDs(0);");
  // Run password forms search to set up unique IDs.
  EXPECT_TRUE(ExecuteJavaScript(@"__gCrWeb.passwords.findPasswordForms();"));

  ExecuteJavaScript(
      @"document.getElementById('u1').value = 'john.doe@gmail.com';");
  [helper_ updateFieldDataOnUserInput:FieldRendererId(1)
                           inputValue:@"john.doe@gmail.com"];

  ExecuteJavaScript(@"document.getElementById('p1').value = 'super!secret';");
  [helper_ updateFieldDataOnUserInput:FieldRendererId(2)
                           inputValue:@"super!secret"];

  // Try to autofill the form.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(BaseUrl(), "", 0, "", 1, "someacc@store.com", "", 2,
                          "store!pw", "", "", NO, &form_data);

  __block bool called = NO;
  __block bool success = NO;
  [helper_ fillPasswordForm:form_data
          completionHandler:^(BOOL res) {
            called = YES;
            success = res;
          }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));
  EXPECT_EQ(success, NO);

  // Make sure that this form can be filled again after a navigation.
  web::FakeNavigationContext context;
  [helper_ webState:web_state() didFinishNavigation:&context];

  success = NO;
  [helper_ fillPasswordForm:form_data
          completionHandler:^(BOOL res) {
            success = res;
          }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return success;
  }));
}

}  // namespace

NS_ASSUME_NONNULL_END
