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

@property(nonatomic, assign) int numberOfReceivedMessage;

@end

@implementation FakeScriptMessageHandlerForForms

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedMessage = message;
  ++_numberOfReceivedMessage;
}

@end

namespace autofill {

namespace {

constexpr int kPostMessageDelayMS = 50;

NSString* TrackFormMutationsJS(int delay, bool allowMessagesBatch) {
  return [NSString
      stringWithFormat:@"__gCrWeb.formHandlers.trackFormMutations(%d, %@)",
                       delay, allowMessagesBatch ? @"true" : @"false"];
}

class AutofillFormHandlersJavascriptTestBase : public web::JavascriptTest {
 protected:
  AutofillFormHandlersJavascriptTestBase()
      : handler_([[FakeScriptMessageHandlerForForms alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandler:handler_
                           name:@"FormHandlersMessage"];
  }
  ~AutofillFormHandlersJavascriptTestBase() override {}

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

class AutofillFormHandlersJavascriptTest
    : public AutofillFormHandlersJavascriptTestBase,
      public testing::WithParamInterface<bool> {};

// Tests that the deletion of a form with a password in it is notified.
TEST_P(AutofillFormHandlersJavascriptTest, NotifyRemovedPasswordForm) {
  NSString* html = @"<html><body><form id=\"form1\">"
                    "<input type=\"password\"></form></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  const bool allowMessagesBatch = GetParam();

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), TrackFormMutationsJS(kPostMessageDelayMS, GetParam()));
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

  if (allowMessagesBatch) {
    EXPECT_NSEQ(@0, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@1, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }

  // Verify that there wasn't another message scheduled. Wait a bit more than
  // `kPostMessageDelayMS` to make sure that it had the opportunity to capture
  // the message.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(
      base::Milliseconds(kPostMessageDelayMS * 2), ^bool() {
        return handler().numberOfReceivedMessage > 1;
      }));
}

// Tests that the deletion of a formless password input element is notified.
TEST_P(AutofillFormHandlersJavascriptTest,
       NotifyRemovedFormlessPasswordElement) {
  // Basic HTML page with the password input element to remove.
  NSString* html =
      @"<html><body><input id=\"input1\" type=\"password\"></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  const bool allowMessagesBatch = GetParam();

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(), TrackFormMutationsJS(kPostMessageDelayMS, GetParam()));

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
  if (allowMessagesBatch) {
    EXPECT_NSEQ(@0, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@1, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }

  // Verify that there wasn't another message scheduled. Wait a bit more than
  // `kPostMessageDelayMS` to make sure that it had the opportunity to capture
  // the message.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(
      base::Milliseconds(kPostMessageDelayMS * 2), ^bool() {
        return handler().numberOfReceivedMessage > 1;
      }));
}

// Tests that removing a form control element and adding a new one in the same
// mutations batch is notified with a message for each mutation, sent
// back-to-back, when batching is enabled.
TEST_P(AutofillFormHandlersJavascriptTest,
       NotifyRemovedAndAddedFormControllerElements) {
  // Basic HTML page in which we add a HTML form.
  NSString* const html = @"<html><body><form id=\"form1\">"
                          "<input type=\"password\"></form></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  const bool allowMessagesBatch = GetParam();

  // Start tracking form mutations with a delay of 50 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(),
      TrackFormMutationsJS(kPostMessageDelayMS, allowMessagesBatch));

  // Make a script to create a new form and replace the old form with it.
  NSString* const replaceFormJS =
      @"const new_form = document.createElement('form'); "
       "new_form.id = 'form2'; "
       "const old_form = document.forms[0]; "
       "old_form.parentNode.replaceChild(new_form, old_form);";

  // Replace the form to trigger an added and a removed form mutation event
  // batched together.
  web::test::ExecuteJavaScript(web_view(), replaceFormJS);

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().numberOfReceivedMessage == 1;
  }));

  NSDictionary* body = nil;

  if (allowMessagesBatch) {
    // Verify the response has all required keys. The removed form message is
    // always the first posted. Skip this when messages batching isn't enabled
    // because the added form comes first in that case.
    body = handler().lastReceivedMessage.body;
    ASSERT_TRUE(body);
    ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
    EXPECT_NSEQ(@"pwdform.removal", body[@"command"]);
    EXPECT_TRUE(body[@"frameID"]);
    EXPECT_TRUE(body[@"formName"]);
    EXPECT_TRUE(body[@"uniqueFormID"]);
    EXPECT_NSEQ(@"", body[@"uniqueFieldID"]);
    EXPECT_NSEQ(nil, body[@"metadata"]);

    // Wait until the notification is pushed and received.
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return handler().numberOfReceivedMessage == 2;
        }));
  }

  // Verify the message for the added form has all required keys for.
  body = handler().lastReceivedMessage.body;
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
  if (allowMessagesBatch) {
    EXPECT_NSEQ(@0, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@2, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }
}

// Tests that once the mutation messages slots are full for a given batch of
// mutations, other mutations are dropped for throttling, when batching is
// enabled.
TEST_P(AutofillFormHandlersJavascriptTest,
       NotifyRemovedAndAddedFormControllerElements_Throttle) {
  // Basic HTML page with 2 password forms and one formless password form.
  NSString* const html = @"<html><body><form id=\"form1\">"
                          "<input type=\"password\"></form>"
                          "<form id=\"form2\"><input type=\"password\"></form>"
                          "<input id=\"input1\" type=\"password\">"
                          "</body></html>";
  ASSERT_TRUE(LoadHtml(html));

  const bool allowMessagesBatch = GetParam();

  // Start tracking form mutations with a delay of `kPostMessageDelayMS`
  // miliseconds to push the mutation message.
  web::test::ExecuteJavaScript(
      web_view(),
      TrackFormMutationsJS(kPostMessageDelayMS, allowMessagesBatch));

  // Make a script that batches 2 messages and ignore all cases once full.
  NSString* const addAndRemoveFormJS =
      @"const parentNode = document.forms[0].parentNode; "
       // Add a generic form and remove a password form, both of which will be
       // notified.
       "parentNode.appendChild(document.createElement('form')); "
       "document.getElementById('form1').remove();"
       // Form transformations from here should be ignored.
       // Add non-password form and remove it, 2 notifications dropped.
       "parentNode.appendChild(document.createElement('form')).remove(); "
       // Remove formless password input, 1 notification dropped.
       "document.getElementById('input1').remove();"
       // Remove password form, 1 notification dropped.
       "document.getElementById('form2').remove();";

  web::test::ExecuteJavaScript(web_view(), addAndRemoveFormJS);

  // Wait until the notification is pushed and received.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
    return handler().numberOfReceivedMessage == 1;
  }));

  NSDictionary* body = nil;

  if (allowMessagesBatch) {
    // Verify the response has all required keys. The removed form message is
    // always the first posted. Skip this when messages batching isn't enabled
    // because the added form comes first in that case.
    body = handler().lastReceivedMessage.body;
    ASSERT_TRUE(body);
    ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
    EXPECT_NSEQ(@"pwdform.removal", body[@"command"]);
    EXPECT_TRUE(body[@"frameID"]);
    EXPECT_TRUE(body[@"formName"]);
    EXPECT_TRUE(body[@"uniqueFormID"]);
    EXPECT_NSEQ(@"", body[@"uniqueFieldID"]);
    // Verify that there is no metadata attached to the first message in the
    // batch where the metadata should be attached to the last message.
    EXPECT_NSEQ(nil, body[@"metadata"]);

    // Wait until the notification is pushed and received.
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return handler().numberOfReceivedMessage == 2;
        }));
  }

  // Verify the message for the added form has all required keys for.
  body = handler().lastReceivedMessage.body;
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
  if (allowMessagesBatch) {
    EXPECT_NSEQ(@4, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@2, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }
  // Verify that there are no more posted messages as the last added and removed
  // form mutations should have been dropped because of throttling. Wait for
  // twice the delay to be sure.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(
      base::Milliseconds(kPostMessageDelayMS * 2), ^bool() {
        // If batching is disabled, there shouldn't be more than 1 message in
        // the batch.
        const int maxMessages = allowMessagesBatch ? 2 : 1;
        return handler().numberOfReceivedMessage > maxMessages;
      }));
}

// Test suite to test all form controls.
INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         AutofillFormHandlersJavascriptTest,
                         ::testing::Values(true, false));

class AutofillFormHandlersJavascriptTestAllHTMLControls
    : public AutofillFormHandlersJavascriptTestBase,
      public testing::WithParamInterface<std::tuple<std::string, bool>> {};

// Tests that adding a form control element is notified as a form changed
// mutation.
TEST_P(AutofillFormHandlersJavascriptTestAllHTMLControls,
       NotifyAddedFormControllerElement) {
  // Basic HTML page in which we add HTML form control elements.
  NSString* html = @"<html><body></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  NSString* elementTag = base::SysUTF8ToNSString(std::get<0>(GetParam()));
  const bool allowMessagesBatch = std::get<1>(GetParam());

  // Start tracking form mutations with a delay of 100 miliseconds to push the
  // mutation message.
  web::test::ExecuteJavaScript(
      web_view(),
      TrackFormMutationsJS(kPostMessageDelayMS, allowMessagesBatch));

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
  if (allowMessagesBatch) {
    EXPECT_NSEQ(@0, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@1, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }
}

// Tests that removing a form control element is notified as a form changed
// mutation.
TEST_P(AutofillFormHandlersJavascriptTestAllHTMLControls,
       NotifyRemovedFormControllerElement) {
  // Basic HTML page in which we add HTML form control elements.
  NSString* html = @"<html><body></body></html>";
  ASSERT_TRUE(LoadHtml(html));

  NSString* elementTag = base::SysUTF8ToNSString(std::get<0>(GetParam()));
  const bool allowMessagesBatch = std::get<1>(GetParam());

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
      web_view(),
      TrackFormMutationsJS(kPostMessageDelayMS, allowMessagesBatch));

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
  if (allowMessagesBatch) {
    EXPECT_NSEQ(@0, body[@"metadata"][@"dropCount"]);
    EXPECT_NSEQ(@1, body[@"metadata"][@"size"]);
  } else {
    // Verify that there is no metadata inserted when using the legacy mutations
    // observer that doesn't allow batching.
    EXPECT_NSEQ(nil, body[@"metadata"]);
  }
}

// Test suite to test all form controls.
INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    AutofillFormHandlersJavascriptTestAllHTMLControls,
    ::testing::Combine(
        ::testing::Values("form", "input", "select", "option", "textarea"),
        ::testing::Bool()));

}  // namespace

}  // namespace autofill
