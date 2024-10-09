// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

const char kExpectedLanguage[] = "Foo";

// Returns an NSString filled with the char 'a' of length `length`.
NSString* GetLongString(NSUInteger length) {
  NSMutableData* data = [[NSMutableData alloc] initWithLength:length];
  memset([data mutableBytes], 'a', length);
  NSString* long_string = [[NSString alloc] initWithData:data
                                                encoding:NSASCIIStringEncoding];
  return long_string;
}

}  // namespace

// A WKScriptMessageHandler which stores the last received WKScriptMessage;
@interface FakeScriptMessageHandler : NSObject <WKScriptMessageHandler>

@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;

@end

@implementation FakeScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedMessage = message;
}

@end

namespace language {

class LanguageDetectionJavascriptTest : public web::JavascriptTest {
 protected:
  LanguageDetectionJavascriptTest()
      : handler_([[FakeScriptMessageHandler alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandler:handler_
                           name:@"LanguageDetectionTextCaptured"];
  }
  ~LanguageDetectionJavascriptTest() override = default;

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
    AddMessageScript();
    AddUserScript(@"language_detection");
  }

  // Triggers Javascript language detection and waits for the received
  // detection results in `handler_.lastReceivedMessage`.
  bool TriggerLanguageDetection() {
    // Reset value to ensure wait below stops at correct time.
    handler_.lastReceivedMessage = nil;

    web::test::ExecuteJavaScript(
        web_view(), @"__gCrWeb.languageDetection.detectLanguage()");
    // Wait until `detectLanguage` completes.
    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
      return handler_.lastReceivedMessage;
    });
  }

  // Retrieves the buffered text content from the language detection script.
  id GetTextContent() {
    return web::test::ExecuteJavaScript(
        web_view(),
        @"__gCrWeb.languageDetection.retrieveBufferedTextContent()");
  }

  FakeScriptMessageHandler* handler() { return handler_; }

 private:
  FakeScriptMessageHandler* handler_;
};

// Tests correctness of the `document.documentElement.lang` attribute.
TEST_F(LanguageDetectionJavascriptTest, HtmlLang) {
  // Non-empty attribute.
  NSString* html = [[NSString alloc]
      initWithFormat:@"<html lang='%s'></html>", kExpectedLanguage];
  ASSERT_TRUE(LoadHtml(html));
  id document_element_lang = web::test::ExecuteJavaScript(
      web_view(), @"document.documentElement.lang;");
  EXPECT_NSEQ(@(kExpectedLanguage), document_element_lang);

  // Empty attribute.
  ASSERT_TRUE(LoadHtml(@"<html></html>"));
  document_element_lang = web::test::ExecuteJavaScript(
      web_view(), @"document.documentElement.lang;");
  EXPECT_NSEQ(@"", document_element_lang);

  // Test with mixed case.
  html = [[NSString alloc]
      initWithFormat:@"<html lAnG='%s'></html>", kExpectedLanguage];
  ASSERT_TRUE(LoadHtml(html));
  document_element_lang = web::test::ExecuteJavaScript(
      web_view(), @"document.documentElement.lang;");
  EXPECT_NSEQ(@(kExpectedLanguage), document_element_lang);
}

// HTML elements introduce a line break, except inline ones.
TEST_F(LanguageDetectionJavascriptTest, ExtractWhitespace) {
  // `b` and `span` do not break lines.
  // `br` and `div` do.
  ASSERT_TRUE(LoadHtml(@"<html><body>"
                        "O<b>n</b>e<br>Two\tT<span>hr</span>ee<div>Four</div>"
                        "</body></html>"));

  ASSERT_TRUE(TriggerLanguageDetection());
  EXPECT_NSEQ(@"One\nTwo\tThree\nFour", GetTextContent());

  // `a` does not break lines.
  // `li`, `p` and `ul` do.
  ASSERT_TRUE(LoadHtml(
      @"<html><body>"
       "<ul><li>One</li><li>T<a href='foo'>wo</a></li></ul><p>Three</p>"
       "</body></html>"));

  ASSERT_TRUE(TriggerLanguageDetection());
  EXPECT_NSEQ(@"\n\nOne\nTwo\nThree", GetTextContent());
}

// Tests that the text content returns only up to `kMaxIndexChars` number of
// characters even if the text content is very large.
TEST_F(LanguageDetectionJavascriptTest, LongTextContent) {
  // Very long string.
  NSUInteger kLongStringLength = kMaxIndexChars - 5;
  NSMutableString* long_string = [GetLongString(kLongStringLength) mutableCopy];
  [long_string appendString:@" b cdefghijklmnopqrstuvwxyz"];

  NSString* html = [[NSString alloc]
      initWithFormat:@"<html><body>%@</html></body>", long_string];
  LoadHtml(html);

  ASSERT_TRUE(TriggerLanguageDetection());
  // The string should be cut at the last whitespace, after the 'b' character.
  EXPECT_EQ(language::kMaxIndexChars, [GetTextContent() length]);
}

// Tests if `__gCrWeb.languageDetection.retrieveBufferedTextContent` correctly
// retrieves the cache and then purges it.
TEST_F(LanguageDetectionJavascriptTest, RetrieveBufferedTextContent) {
  LoadHtml(@"<html>foo</html>");

  // Set cached text content by running language detection.
  ASSERT_TRUE(TriggerLanguageDetection());
  // Retrieve the content which should clear the cache.
  EXPECT_NSEQ(@"foo", GetTextContent());
  // Verify cache is purged.
  EXPECT_NSEQ([NSNull null], GetTextContent());
}

// Tests that the LanguageDetectionTextCaptured message body correctly informs
// the native side of page state.
TEST_F(LanguageDetectionJavascriptTest,
       LanguageDetectionTextCapturedResponseBody) {
  LoadHtml(@"<html></html>");
  ASSERT_TRUE(TriggerLanguageDetection());

  // Verify the response has all required keys.
  NSDictionary* body = handler().lastReceivedMessage.body;
  ASSERT_TRUE(body);
  ASSERT_TRUE([body isKindOfClass:[NSDictionary class]]);
  EXPECT_TRUE(body[@"frameId"]);
  EXPECT_TRUE(body[@"hasNoTranslate"]);
  EXPECT_TRUE(body[@"htmlLang"]);
  EXPECT_TRUE(body[@"httpContentLanguage"]);
}

// Tests if `__gCrWeb.languageDetection.detectLanguage` correctly informs the
// native side when the notranslate meta tag is specified.
TEST_F(LanguageDetectionJavascriptTest, DetectLanguageWithNoTranslateMeta) {
  // A simple page using the notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='foo'>"
                   @"<meta name='google' content='notranslate'>"
                   @"</head></html>";
  LoadHtml(html);
  ASSERT_TRUE(TriggerLanguageDetection());

  ASSERT_TRUE(handler().lastReceivedMessage.body[@"httpContentLanguage"]);
  EXPECT_NSEQ(@"foo",
              handler().lastReceivedMessage.body[@"httpContentLanguage"]);
  ASSERT_TRUE(handler().lastReceivedMessage.body[@"hasNoTranslate"]);
  EXPECT_TRUE(
      [handler().lastReceivedMessage.body[@"hasNoTranslate"] boolValue]);
}

// Tests if `__gCrWeb.languageDetection.detectLanguage` correctly informs the
// native side when no notranslate meta tag is specified.
TEST_F(LanguageDetectionJavascriptTest, DetectLanguageWithoutNoTranslateMeta) {
  // A simple page using the notranslate meta tag.
  NSString* html = @"<html><head>"
                   @"<meta http-equiv='content-language' content='foo'>"
                   @"</head></html>";
  LoadHtml(html);
  ASSERT_TRUE(TriggerLanguageDetection());

  ASSERT_TRUE(handler().lastReceivedMessage.body[@"httpContentLanguage"]);
  EXPECT_NSEQ(@"foo",
              handler().lastReceivedMessage.body[@"httpContentLanguage"]);
  ASSERT_TRUE(handler().lastReceivedMessage.body[@"hasNoTranslate"]);
  EXPECT_FALSE(
      [handler().lastReceivedMessage.body[@"hasNoTranslate"] boolValue]);
}

}  // namespace language
