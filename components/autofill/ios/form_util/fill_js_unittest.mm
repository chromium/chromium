// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stddef.h>

#import <array>

#import "base/apple/foundation_util.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/cross_content_world_util_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace autofill {

// Creates a dummy JavaScriptFeature for the page content world.
// Used for running test scripts in the page content world.
web::JavaScriptFeature* GetDummyPageContentWorldFeature() {
  static base::NoDestructor<web::JavaScriptFeature> dummy_feature(
      web::ContentWorld::kPageContentWorld,
      /*feature_scripts=*/std::vector<web::JavaScriptFeature::FeatureScript>());
  return dummy_feature.get();
}

// Creates a dummy JavaScriptFeature for the isolated content world.
// Used for running test scripts in the isolated content world.
web::JavaScriptFeature* GetDummyIsolatedWorldFeature() {
  static base::NoDestructor<web::JavaScriptFeature> dummy_feature(
      web::ContentWorld::kIsolatedWorld,
      /*feature_scripts=*/std::vector<web::JavaScriptFeature::FeatureScript>());
  return dummy_feature.get();
}

// Retuns the dummy JS feature for the corresponding content world.
web::JavaScriptFeature* GetDummyFeatureForContentWorld(
    web::ContentWorld content_world) {
  switch (content_world) {
    case web::ContentWorld::kIsolatedWorld:
      return GetDummyIsolatedWorldFeature();
    case web::ContentWorld::kPageContentWorld:
      return GetDummyPageContentWorldFeature();
    case web::ContentWorld::kAllContentWorlds:
      NOTREACHED_NORETURN();
  }
}

// TODO(crbug.com/359538514): Make test non-parametrized once Autofill in the
// isolated world is launched.
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

    form_util_java_script_feature_ = new FormUtilJavaScriptFeature();

    OverrideJavaScriptFeatures(
        {form_util_java_script_feature(),
         CrossContentWorldUtilJavaScriptFeature::GetInstance(),
         GetDummyPageContentWorldFeature(), GetDummyIsolatedWorldFeature()});
  }

  void TearDown() override {
    // Clean up overriden features. Don't leave a dangling pointer to
    // `FormUtilJavaScriptFeature`.
    OverrideJavaScriptFeatures({});
    delete form_util_java_script_feature_;
    web::WebTestWithWebState::TearDown();
  }

  FormUtilJavaScriptFeature* form_util_java_script_feature() {
    return form_util_java_script_feature_;
  }

 protected:
  // Propagates the status of kAutofillIsolatedWorldForJavascriptIos to the
  // renderer.
  void EnableCrossContentWorldAutofill() {
    NSString* enable_feature_script =
        [NSString stringWithFormat:@"__gCrWeb.autofill_form_features."
                                   @"setAutofillIsolatedContentWorld(%@);",
                                   GetParam() ? @"true" : @"false"];
    web::test::ExecuteJavaScriptForFeature(web_state(), enable_feature_script,
                                           form_util_java_script_feature());
  }

  // Returns the chrome-set renderer ID for the element with ID `element_id`.
  // Runs gCrWeb.fill.getUniqueID in the given content world.
  NSString* GetUniqueID(NSString* element_id, web::ContentWorld content_world) {
    NSString* script = [NSString
        stringWithFormat:
            @"__gCrWeb.fill.getUniqueID(document.getElementById('%@'))",
            element_id];

    id result_id = web::test::ExecuteJavaScriptForFeature(
        web_state(), script, GetDummyFeatureForContentWorld(content_world));
    return base::apple::ObjCCastStrict<NSString>(result_id);
  }

  base::test::ScopedFeatureList feature_list_;
  // Test instance of FormUtilJavaScriptFeature. Using a pointer so we can
  // create the instance after the feature flag for isolated world is set in
  // `SetUp`.
  // TODO(crbug.com/359538514): Remove this variable and use
  // FormUtilJavaScriptFeature::GetInstance() once Autofill in the isolated
  // world is launched.
  FormUtilJavaScriptFeature* form_util_java_script_feature_ = nullptr;
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

// Tests that gCrWeb.fill.getUniqueID returns the ID of an element from all
// JavaScript content worlds.
TEST_P(FillJsTest, GetUniqueIDInAllJavaScriptContentWorlds) {
  LoadHtml(@"<html><body>"
            "<form id='form'>"
            "<input id='input' type='text'></input>"
            "</form></body></html>");

  EnableCrossContentWorldAutofill();

  // Set IDs for form and input in the content world for Autofill features.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"var form = document.getElementById('form');"
       "__gCrWeb.fill.setUniqueIDIfNeeded(form);"
       "var input = document.getElementById('input');"
       "__gCrWeb.fill.setUniqueIDIfNeeded(input);",
      form_util_java_script_feature());

  // Verify the ID retrieval in all content worlds.
  for (auto content_world : {web::ContentWorld::kIsolatedWorld,
                             web::ContentWorld::kPageContentWorld}) {
    bool is_autofill_world =
        ContentWorldForAutofillJavascriptFeatures() == content_world;
    SCOPED_TRACE(testing::Message()
                 << "Autofill content world = " << is_autofill_world);
    // Check that the correct ID is returned for the form and input elements.
    // IDs should accessible from both content worlds when the flag is on.
    // When the flag is off, the IDs are accessible only from the same content
    // world other Autofill scripts are injected in.
    id form_id = GetUniqueID(@"form", content_world);
    EXPECT_NSEQ(form_id, GetParam() || is_autofill_world ? @"1" : @"0");

    id input_id = GetUniqueID(@"input", content_world);
    EXPECT_NSEQ(input_id, GetParam() || is_autofill_world ? @"2" : @"0");
  }
}

// Tests that gCrWeb.fill.getUniqueID returns the null ID when an invalid value
// is stored in the DOM.
TEST_P(FillJsTest, GetUniqueIDReturnsNotSetWhenInvalidIDInDOM) {
  LoadHtml(@"<html><body>"
            "<form id='form'/>"
            "</form></body></html>");

  EnableCrossContentWorldAutofill();

  // Set IDs for form and input in the content world for Autofill features.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"var form = document.getElementById('form');"
       "__gCrWeb.fill.setUniqueIDIfNeeded(form);",
      form_util_java_script_feature());

  std::vector<NSString*> invalid_ids = {@"''", @"'word'", @"null",
                                        @"undefined"};

  for (NSString* invalid_id : invalid_ids) {
    SCOPED_TRACE(testing::Message() << "invalid_id = " << invalid_id);
    NSString* set_invalid_id_script = [NSString
        stringWithFormat:@"var form = document.getElementById('form');"
                          "form.setAttribute('__gChrome_uniqueID', %@);",
                         invalid_id];

    web::test::ExecuteJavaScriptForFeature(web_state(), set_invalid_id_script,
                                           form_util_java_script_feature());

    // Verify the ID retrieval in all content worlds.
    for (auto content_world : {web::ContentWorld::kIsolatedWorld,
                               web::ContentWorld::kPageContentWorld}) {
      bool is_autofill_world =
          ContentWorldForAutofillJavascriptFeatures() == content_world;
      SCOPED_TRACE(testing::Message()
                   << "Autofill content world = " << is_autofill_world);
      // The ID should be non-zero only in the same content world as the rest of
      // Autofill scripts. In the other content world, the ID stored in the DOM
      // is invalid so getUniqueID should return the null/zero ID.
      id form_id = GetUniqueID(@"form", content_world);
      EXPECT_NSEQ(form_id, is_autofill_world ? @"1" : @"0");
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*No InstantiationName*/,
    FillJsTest,
    /*enable kAutofillIsolatedWorldForJavascriptIos*/ testing::Bool());

}  // namespace autofill
