// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// A WKScriptMessageHandler which stores the last received WKScriptMessage;
@interface FakeScriptMessageHandlerForForms : NSObject <WKScriptMessageHandler>

@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;

@end

@implementation FakeScriptMessageHandlerForForms

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedMessage = message;
}

@end

namespace autofill {

namespace {

class AutofillFormHandlersJavascriptTest : public web::JavascriptTest {
 protected:
  AutofillFormHandlersJavascriptTest()
      : handler_([[FakeScriptMessageHandlerForForms alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandler:handler_
                           name:@"FormHandlersMessage"];
  }
  ~AutofillFormHandlersJavascriptTest() override {}

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
    AddMessageScript();

    AddUserScript(@"form_handlers");
    AddUserScript(@"fill");
    AddUserScript(@"form");
  }

  FakeScriptMessageHandlerForForms* handler() { return handler_; }

 private:
  FakeScriptMessageHandlerForForms* handler_;
};

// Tests that the deletion of a form with a password in it is notified.
TEST_F(AutofillFormHandlersJavascriptTest, NotifyRemovedPasswordForm) {
  NSString* html = @"<html><body><form id=\"form1\">"
                    "<input type=\"password\"></form></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.formHandlers.trackFormMutations(100)");
  // Trigger a remove password form mutation event.
  web::test::ExecuteJavaScript(web_view(), @"document.forms[0].remove()");

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().lastReceivedMessage != nil;
  }));

  // Verify the response has all required keys.
  NSDictionary* body = handler().lastReceivedMessage.body;
  ASSERT_TRUE(body);
  ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"pwdform.removal", body[@"command"]);
  EXPECT_TRUE(body[@"frameID"]);
  EXPECT_TRUE(body[@"formName"]);
  EXPECT_TRUE(body[@"uniqueFormID"]);
  EXPECT_NSEQ(@"", body[@"uniqueFieldID"]);
}

// Tests that the deletion of a formless password input element is notified.
TEST_F(AutofillFormHandlersJavascriptTest,
       NotifyRemovedFormlessPasswordElement) {
  // Basic HTML page with the password input element to remove.
  NSString* html =
      @"<html><body><input id=\"input1\" type=\"password\"></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.formHandlers.trackFormMutations(100)");

  // Remove the password input element.
  NSString* js = @"document.getElementById('input1').remove();";
  web::test::ExecuteJavaScript(web_view(), js);

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().lastReceivedMessage != nil;
  }));

  // Verify the response has all required keys.
  NSDictionary* body = handler().lastReceivedMessage.body;
  ASSERT_TRUE(body);
  ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"pwdform.removal", body[@"command"]);
  ASSERT_TRUE(body[@"frameID"]);
  EXPECT_NSEQ(@"", body[@"formName"]);
  EXPECT_NSEQ(@"", body[@"uniqueFormID"]);
  EXPECT_TRUE(body[@"uniqueFieldID"]);
}

class AutofillFormHandlersJavascriptTestAllHTMLControls
    : public AutofillFormHandlersJavascriptTest,
      public testing::WithParamInterface<std::string> {};

// Tests that adding a form control element is notified as a form changed
// mutation.
TEST_P(AutofillFormHandlersJavascriptTestAllHTMLControls,
       NotifyAddedFormControllerElement) {
  // Basic HTML page in which we add HTML form control elements.
  NSString* html = @"<html><body></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  NSString* elementTag = base::SysUTF8ToNSString(GetParam());

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.formHandlers.trackFormMutations(100)");

  // Add the HTML form control element to the content.
  NSString* js = [[NSString alloc]
      initWithFormat:
          @"document.body.appendChild(document.createElement('%@'));",
          elementTag];
  web::test::ExecuteJavaScript(web_view(), js);

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().lastReceivedMessage != nil;
  }));

  // Verify the response has all required keys.
  NSDictionary* body = handler().lastReceivedMessage.body;
  ASSERT_TRUE(body);
  ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"form.activity", body[@"command"]);
  EXPECT_TRUE(body[@"frameID"]);
  EXPECT_NSEQ(@"", body[@"formName"]);
  EXPECT_NSEQ(@"", body[@"uniqueFormID"]);
  EXPECT_NSEQ(@"", body[@"fieldIdentifier"]);
  EXPECT_NSEQ(@"", body[@"uniqueFieldID"]);
  EXPECT_NSEQ(@"", body[@"fieldType"]);
  EXPECT_NSEQ(@"form_changed", body[@"type"]);
  EXPECT_NSEQ(@"", body[@"value"]);
  EXPECT_NSEQ(@NO, body[@"hasUserGesture"]);
}

// Tests that removing a form control element is notified as a form changed
// mutation.
TEST_P(AutofillFormHandlersJavascriptTestAllHTMLControls,
       NotifyRemovedFormControllerElement) {
  // Basic HTML page in which we add HTML form control elements.
  NSString* html = @"<html><body></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  NSString* elementTag = base::SysUTF8ToNSString(GetParam());

  // Add the HTML form control element to the content.
  NSString* addElemJS = [NSString
      stringWithFormat:
          @"const e = document.body.appendChild(document.createElement('%@'));"
           " e.id = 'element-id'",
          elementTag];
  web::test::ExecuteJavaScript(web_view(), addElemJS);

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.formHandlers.trackFormMutations(100)");

  // Remove the form control element to trigger the mutation notification.
  web::test::ExecuteJavaScript(
      web_view(), @"document.getElementById('element-id').remove();");

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().lastReceivedMessage != nil;
  }));

  // Verify the response has all required keys.
  NSDictionary* body = handler().lastReceivedMessage.body;
  ASSERT_TRUE(body);
  ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"form.activity", body[@"command"]);
  EXPECT_TRUE(body[@"frameID"]);
  EXPECT_NSEQ(@"", body[@"formName"]);
  EXPECT_NSEQ(@"", body[@"uniqueFormID"]);
  EXPECT_NSEQ(@"", body[@"fieldIdentifier"]);
  EXPECT_NSEQ(@"", body[@"uniqueFieldID"]);
  EXPECT_NSEQ(@"", body[@"fieldType"]);
  EXPECT_NSEQ(@"form_changed", body[@"type"]);
  EXPECT_NSEQ(@"", body[@"value"]);
  EXPECT_NSEQ(@NO, body[@"hasUserGesture"]);
}

// Test suite to test all form controls.
INSTANTIATE_TEST_SUITE_P(
    AutofillFormHandlersJavascriptTest,
    AutofillFormHandlersJavascriptTestAllHTMLControls,
    ::testing::Values("form", "input", "select", "option", "textarea"));

}  // namespace

}  // namespace autofill
