// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"

using web::test::ExecuteJavaScript;
using web::test::ExecuteJavaScriptInWebView;

NSString* const kMessageHandlerName = @"TestHandler";

//
@interface FakeScriptMessageHandlerForFormTesting
    : NSObject <WKScriptMessageHandler>

@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@property(nonatomic, assign) int messageCount;

@end

@implementation FakeScriptMessageHandlerForFormTesting

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedMessage = message;
  ++_messageCount;
}

@end

namespace autofill {

// Text fixture to test form.js.
class FormJsTest : public web::JavascriptTest {
 protected:
  FormJsTest()
      : handler_([[FakeScriptMessageHandlerForFormTesting alloc] init]) {
    // Add an handler to get messages.
    [web_view().configuration.userContentController
        addScriptMessageHandler:handler_
                           name:kMessageHandlerName];
  }

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddWebViewToKeyWindow();

    // Inject default feature flag placeholders before loading scripts.
    WKUserScript* script = [[WKUserScript alloc]
          initWithSource:autofill::test::GetAutofillTestPlaceholders()
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:NO];
    [web_view().configuration.userContentController addUserScript:script];

    AddGCrWebScript();
    AddUserScript(@"autofill_form_features");
    AddUserScript(@"form_util_tests");
  }

  void TearDown() override {
    [web_view() removeFromSuperview];
    web::JavascriptTest::TearDown();
  }

  void AddWebViewToKeyWindow() {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
      UIWindowScene* window_scene = base::apple::ObjCCast<UIWindowScene>(scene);
      if (!window_scene) {
        continue;
      }
      for (UIWindow* window in window_scene.windows) {
        if (window.isKeyWindow) {
          [window.rootViewController.view addSubview:web_view()];
          return;
        }
      }
    }
  }

  bool LoadHtml(NSString* html) {
    WKWebView* web_view_instance = web_view();
    for (int attempt = 0; attempt < 3; ++attempt) {
      if (web::JavascriptTest::LoadHtml(html) &&
          base::test::ios::WaitUntilConditionOrTimeout(
              base::test::ios::kWaitForPageLoadTimeout, ^{
                return !web_view_instance.loading &&
                       [ExecuteJavaScript(web_view_instance,
                                          @"typeof __gCrWeb !== 'undefined' && "
                                          @"document.readyState === 'complete'")
                           isEqual:@YES];
              })) {
        return true;
      }
    }
    return false;
  }

  FakeScriptMessageHandlerForFormTesting* handler_;
};

TEST_F(FormJsTest, GetIframeElements) {
  ASSERT_TRUE(LoadHtml(@"<iframe id='frame1' srcdoc='foo'></iframe>"
                       @"<p id='not-an-iframe'>"
                       @"<iframe id='frame2' srcdoc='bar'></iframe>"
                       @"<marquee id='definitely-not-an-iframe'>baz</marquee>"
                       @"</p>"));

  EXPECT_NSEQ(
      @"frame1,frame2",
      ExecuteJavaScript(web_view(),
                        @"const frames = "
                        @"__gCrWeb.getRegisteredApi('form_test_api')."
                        @"getFunction('getIframeElements')(document.body);"
                        @"frames.map((f) => { return f.id; }).join();"));

  // Check that the return objects have a truthy contentWindow property.
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(web_view(), @"!!(frames[0].contentWindow);"));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(web_view(), @"!!(frames[1].contentWindow);"));
}

// Tests that the `formSubmitted` handler does deduping.
TEST_F(FormJsTest, FormSubmitted_Deduping) {
  // Create arbitrary form elements to use as the submitted forms.
  ASSERT_TRUE(
      LoadHtml(@"<form name='form1'></form><form name='form2'></form>"));

  // Swizzle the webkit messaging posting method to count the number of messages
  // sent over. It should only concern submission messages for this test.
  NSString* swizzleScript =
      @"var gMsgCount = 0; "
       "let oldFn = UserMessageHandler.prototype.postMessage; "
       " function newFn(...args) { ++gMsgCount; return oldFn.apply(this, "
       "args); }; "
       "UserMessageHandler.prototype.postMessage = newFn; "
       "void(0);";
  ExecuteJavaScriptInWebView(web_view(), swizzleScript);

  // == Submit first form ==

  // Submit the first form for the first time.
  ExecuteJavaScriptInWebView(web_view(),
                             @"__gCrWeb.getRegisteredApi('form_test_api')."
                             @"getFunction('formSubmitted')(document.forms[0], "
                             @"'TestHandler', false, false)");

  // Wait for the submission message for the first form to be received from the
  // renderer. This verifies that the submission is at least reported once.
  {
    __block WKScriptMessage* messageFromForm;
    __weak __typeof(handler_) weak_handler = handler_;
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
          messageFromForm = weak_handler.lastReceivedMessage;
          return weak_handler.lastReceivedMessage;
        }));
    ASSERT_TRUE(messageFromForm);
    NSDictionary* messageFromFormContent =
        base::apple::ObjCCastStrict<NSDictionary>(messageFromForm.body);
    EXPECT_NSEQ(@"form1", messageFromFormContent[@"formName"]);
  }

  // Attempt other submissions on the same form, where it should be deduped
  // this time, hence ignored.
  for (size_t i = 0; i < 4; ++i) {
    ExecuteJavaScriptInWebView(web_view(),
                               @"__gCrWeb.getRegisteredApi('form_test_api')."
                               @"getFunction('formSubmitted')(document.forms[0]"
                               @", 'TestHandler', false, false)");
  }

  // Verify that the submission message was only sent over once despite
  // triggering formSubmitted() 5 times on the same form (the first form in this
  // occurrence). Since all the scripts are run in order in the same JS event
  // loop, it is guaranteed that all formSubmitted() were made before verifying
  // the number of calls.
  EXPECT_NSEQ(@(1), ExecuteJavaScript(web_view(), @"gMsgCount"));

  handler_.lastReceivedMessage = nil;

  // == Submit other form ==

  // Submit the other form that wasn't submitted yet.
  ExecuteJavaScriptInWebView(web_view(),
                             @"__gCrWeb.getRegisteredApi('form_test_api')."
                             @"getFunction('formSubmitted')(document.forms[1], "
                             @"'TestHandler', false, false)");

  // Wait for the submission message for the other form to be received from the
  // renderer. This verifies that the submission is at least reported once per
  // form.
  {
    __weak __typeof(handler_) weak_handler = handler_;
    __block WKScriptMessage* messageFromForm;
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
          messageFromForm = weak_handler.lastReceivedMessage;
          return weak_handler.lastReceivedMessage;
        }));
    ASSERT_TRUE(messageFromForm);
    NSDictionary* messageFromFormContent =
        base::apple::ObjCCastStrict<NSDictionary>(messageFromForm.body);
    EXPECT_NSEQ(@"form2", messageFromFormContent[@"formName"]);
  }

  // Attempt other submissions on the same form, where it should be deduped
  // this time, hence ignored. Verify that the submission message count remains
  // 2, one message for each form.
  for (size_t i = 0; i < 4; ++i) {
    ExecuteJavaScriptInWebView(web_view(),
                               @"__gCrWeb.getRegisteredApi('form_test_api')."
                               @"getFunction('formSubmitted')(document.forms[1]"
                               @", 'TestHandler', false, false)");
  }
  EXPECT_TRUE(ExecuteJavaScript(web_view(), @"gMsgCount == 2"));
}

// Tests that calling `setUniqueIDIfNeeded` on an element that was already
// assigned a node ID via `getOrCreateUniqueNodeId` (e.g. during APC extraction)
// still populates the `__gCrUniqueID` DOM attribute, registers the element in
// `getElementByUniqueID`, and tracks password fields.
TEST_F(FormJsTest, SetUniqueIDIfNeeded_AfterGetOrCreateUniqueNodeId) {
  ASSERT_TRUE(LoadHtml(@"<form id='form'>"
                       @"<input id='password_input' type='password'>"
                       @"</form>"));

  // Simulate APC assigning a node ID before Autofill runs setUniqueIDIfNeeded.
  EXPECT_NSEQ(@1,
              ExecuteJavaScript(
                  web_view(),
                  @"const input = document.getElementById('password_input');"
                  @"const api = __gCrWeb.getRegisteredApi('form_test_api');"
                  @"api.getFunction('getOrCreateUniqueNodeId')(input);"));

  // Before setUniqueIDIfNeeded runs, Autofill registration has not happened.
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript(
          web_view(), @"input.getAttribute('__gCrUniqueID') === null && "
                      @"api.getFunction('getElementByUniqueID')(1) === null && "
                      @"!input[Symbol.for('__gCrHasBeenPassword')];"));

  // Now run Autofill's setUniqueIDIfNeeded on the same input.
  ExecuteJavaScriptInWebView(web_view(),
                             @"api.getFunction('setUniqueIDIfNeeded')(input);");

  EXPECT_NSEQ(@"1", ExecuteJavaScript(web_view(),
                                      @"input.getAttribute('__gCrUniqueID');"));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(
                  web_view(),
                  @"api.getFunction('getElementByUniqueID')(1) === input;"));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript(
          web_view(), @"input[Symbol.for('__gCrHasBeenPassword')] === true;"));
}

// Tests that `setUniqueIDIfNeeded` succeeds on an HTMLFormElement even when a
// child input named "setAttribute" shadows `form.setAttribute`.
TEST_F(FormJsTest, SetUniqueIDIfNeeded_ClobberedSetAttribute) {
  ASSERT_TRUE(LoadHtml(@"<form id='form'>"
                       @"<input name='setAttribute'>"
                       @"</form>"));

  ExecuteJavaScriptInWebView(
      web_view(), @"const form = document.getElementById('form');"
                  @"const api = __gCrWeb.getRegisteredApi('form_test_api');"
                  @"api.getFunction('setUniqueIDIfNeeded')(form);");

  EXPECT_NSEQ(@"1", ExecuteJavaScript(web_view(),
                                      @"Element.prototype.getAttribute.call("
                                      @"form, '__gCrUniqueID');"));
  EXPECT_NSEQ(
      @YES,
      ExecuteJavaScript(
          web_view(), @"api.getFunction('getElementByUniqueID')(1) === form;"));
}

}  // namespace autofill
