// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"

using web::test::ExecuteJavaScript;

namespace autofill {

// Text fixture to test form.js.
class FormJsTest : public web::JavascriptTest {
 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();
    AddUserScript(@"fill");
    AddUserScript(@"form");
  }
};

TEST_F(FormJsTest, GetIframeElements) {
  LoadHtml(@"<iframe id='frame1' srcdoc='foo'></iframe>"
           @"<p id='not-an-iframe'>"
           @"<iframe id='frame2' srcdoc='bar'></iframe>"
           @"<marquee id='definitely-not-an-iframe'>baz</marquee>"
           @"</p>");

  EXPECT_NSEQ(
      @"frame1,frame2",
      ExecuteJavaScript(
          web_view(),
          @"const frames = __gCrWeb.form.getIframeElements(document.body);"
          @"frames.map((f) => { return f.id; }).join();"));

  // Check that the return objects have a truthy contentWindow property.
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(web_view(), @"!!(frames[0].contentWindow);"));
  EXPECT_NSEQ(@YES,
              ExecuteJavaScript(web_view(), @"!!(frames[1].contentWindow);"));
}

}  // namespace autofill
