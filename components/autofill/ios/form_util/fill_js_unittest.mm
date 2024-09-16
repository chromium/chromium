// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stddef.h>

#import <array>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/cross_content_world_util_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace autofill {

class FillJsTest : public web::WebTestWithWebState,
                   public ::testing::WithParamInterface<bool> {
 public:
  FillJsTest() : web::WebTestWithWebState() {}

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    // Enable isolated autofill flag before the JS feature is instantiated. This
    // way the scripts are injected in the right content world.
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          kAutofillIsolatedWorldForJavascriptIos);
    }

    OverrideJavaScriptFeatures(
        {form_util_java_script_feature(),
         autofill::CrossContentWorldUtilJavaScriptFeature::GetInstance()});
  }

  void TearDown() override {
    // Clean up overriden features. Don't leave a dangling pointer to
    // `form_util_java_script_feature`.
    OverrideJavaScriptFeatures({});
    web::WebTestWithWebState::TearDown();
  }

  FormUtilJavaScriptFeature* form_util_java_script_feature() {
    return &form_util_java_script_feature_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  // Test instance of FormUtilJavaScriptFeature.
  FormUtilJavaScriptFeature form_util_java_script_feature_;
};

TEST_P(FillJsTest, GetCanonicalActionForForm) {
  struct TestData {
    NSString* html_action;
    NSString* expected_action;
  };
  const auto test_data = std::to_array<TestData>(
      {// Empty action.
       TestData{nil, @"baseurl/"},
       // Absolute urls.
       TestData{@"http://foo1.com/bar", @"http://foo1.com/bar"},
       TestData{@"http://foo2.com/bar#baz", @"http://foo2.com/bar"},
       TestData{@"http://foo3.com/bar?baz", @"http://foo3.com/bar"},
       // Relative urls.
       TestData{@"action.php", @"baseurl/action.php"},
       TestData{@"action.php?abc", @"baseurl/action.php"},
       // Non-http protocols.
       TestData{@"data:abc", @"data:abc"},
       TestData{@"javascript:login()", @"javascript:login()"}});

  for (size_t i = 0; i < test_data.size(); i++) {
    const TestData& data = test_data[i];
    NSString* html_action =
        data.html_action == nil
            ? @""
            : [NSString stringWithFormat:@"action='%@'", data.html_action];
    NSString* html = [NSString stringWithFormat:@"<html><body>"
                                                 "<form %@></form>"
                                                 "</body></html>",
                                                html_action];

    LoadHtml(html);
    id result = web::test::ExecuteJavaScriptForFeature(
        web_state(),
        @"__gCrWeb.fill.getCanonicalActionForForm(document.body.children[0])",
        form_util_java_script_feature());
    NSString* base_url = base::SysUTF8ToNSString(BaseUrl());
    NSString* expected_action =
        [data.expected_action stringByReplacingOccurrencesOfString:@"baseurl/"
                                                        withString:base_url];
    EXPECT_NSEQ(expected_action, result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.html_action);
  }
}

// Tests the extraction of the aria-label attribute.
TEST_P(FillJsTest, GetAriaLabel) {
  LoadHtml(@"<input id='input' type='text' aria-label='the label'/>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"the label";
  EXPECT_NSEQ(result, expected_result);
}

// Tests if shouldAutocomplete returns valid result for
// autocomplete='one-time-code'.
TEST_F(FillJsTest, ShouldAutocompleteOneTimeCode) {
  LoadHtml(@"<input id='input' type='text' autocomplete='one-time-code'/>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.shouldAutocomplete(document.getElementById('input'));",
      form_util_java_script_feature());
  EXPECT_NSEQ(result, @NO);
}

// Tests that aria-labelledby works. Simple case: only one id referenced.
TEST_F(FillJsTest, GetAriaLabelledBySingle) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-labelledby='name'/>"
            "</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"Name";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-labelledby works: Complex case: multiple ids referenced.
TEST_P(FillJsTest, GetAriaLabelledByMulti) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-labelledby='billing name'/>"
            "</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"Billing Name";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-labelledby takes precedence over aria-label
TEST_F(FillJsTest, GetAriaLabelledByTakesPrecedence) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-label='ignored' "
            "         aria-labelledby='name'/>"
            "</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"Name";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that an invalid aria-labelledby reference gets ignored (as opposed to
// crashing, for example).
TEST_P(FillJsTest, GetAriaLabelledByInvalid) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-labelledby='div1 div2'/>"
            "</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that invalid aria-labelledby references fall back to aria-label.
TEST_P(FillJsTest, GetAriaLabelledByFallback) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-label='valid' "
            "          aria-labelledby='div1 div2'/>"
            "</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaLabel(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"valid";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-describedby works: Simple case: a single id referenced.
TEST_P(FillJsTest, GetAriaDescriptionSingle) {
  LoadHtml(@"<html><body>"
            "<input id='input' type='text' aria-describedby='div1'/>"
            "<div id='div1'>aria description</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaDescription(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"aria description";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-describedby works: Complex case: multiple ids referenced.
TEST_F(FillJsTest, GetAriaDescriptionMulti) {
  LoadHtml(@"<html><body>"
            "<input id='input' type='text' aria-describedby='div1 div2'/>"
            "<div id='div2'>description</div>"
            "<div id='div1'>aria</div>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaDescription(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"aria description";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that invalid aria-describedby returns the empty string.
TEST_P(FillJsTest, GetAriaDescriptionInvalid) {
  LoadHtml(@"<html><body>"
            "<input id='input' type='text' aria-describedby='invalid'/>"
            "</body></html>");

  id result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.fill.getAriaDescription(document.getElementById('input'));",
      form_util_java_script_feature());
  NSString* expected_result = @"";
  EXPECT_NSEQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    /*No InstantiationName*/,
    FillJsTest,
    /*enable kAutofillIsolatedWorldForJavascriptIos*/ testing::Bool());

}  // namespace autofill
