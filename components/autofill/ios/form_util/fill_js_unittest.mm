// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stddef.h>

#import <array>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace autofill {

// Struct for stringify() test data.
struct TestScriptAndExpectedValue {
  NSString* test_script;
  id expected_value;
};

// Creates a JavaScriptFeature that injects fill util functions used in tests.
web::JavaScriptFeature::FeatureScript GetFillTestScript() {
  return web::JavaScriptFeature::FeatureScript::CreateWithFilename(
      "fill_util_test",
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames);
}

// Creates a JavaScriptFeature that injects `fill_util_test.ts` into
// `content_world`.
std::unique_ptr<web::JavaScriptFeature> CreateFillTestFeature(
    web::ContentWorld content_world) {
  return std::make_unique<web::JavaScriptFeature>(
      content_world,
      /*feature_scripts=*/
      std::vector<web::JavaScriptFeature::FeatureScript>({GetFillTestScript()}),
      /*dependent_features=*/
      std::vector<const web::JavaScriptFeature*>(
          {// `fill_util_test.ts` indirectly depends of
           // `autofill_form_features.ts`.
           AutofillFormFeaturesJavaScriptFeature::GetInstance()}));
}

class FillJsTest : public AutofillTestWithWebState {
 protected:
  FillJsTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    isolated_world_feature_ =
        CreateFillTestFeature(web::ContentWorld::kIsolatedWorld);
    page_world_feature_ =
        CreateFillTestFeature(web::ContentWorld::kPageContentWorld);

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {isolated_world_feature_.get(), page_world_feature_.get()});
  }

  // Returns the chrome-set renderer ID for the element with ID `element_id`.
  // Runs getUniqueID from fill_test_api API in the given content world.
  NSString* GetUniqueID(NSString* element_id, web::ContentWorld content_world) {
    NSString* script = [NSString
        stringWithFormat:
            @"__gCrWeb.getRegisteredApi('fill_test_api')."
            @"getFunction('getUniqueID')(document.getElementById('%@'))",
            element_id];

    id result_id = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
        web_state(), script, GetTestFeatureForContentWorld(content_world));

    return base::apple::ObjCCastStrict<NSString>(result_id);
  }

  // Runs `script` in the isolated content world..
  id ExecuteJavaScript(NSString* script) override {
    return web::test::ExecuteJavaScriptForFeatureAndReturnResult(
        web_state(), script, isolated_world_feature_.get());
  }

 private:
  // Retuns the test JS feature for the corresponding content world.
  web::JavaScriptFeature* GetTestFeatureForContentWorld(
      web::ContentWorld content_world) {
    switch (content_world) {
      case web::ContentWorld::kIsolatedWorld:
        return isolated_world_feature_.get();
      case web::ContentWorld::kPageContentWorld:
        return page_world_feature_.get();
      case web::ContentWorld::kAllContentWorlds:
        NOTREACHED();
    }
  }

  std::unique_ptr<web::JavaScriptFeature> isolated_world_feature_;
  std::unique_ptr<web::JavaScriptFeature> page_world_feature_;
};

TEST_F(FillJsTest, GetCanonicalActionForForm) {
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

  for (size_t i = 0; i < test_data.size(); ++i) {
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
    id result = ExecuteJavaScript(
        @"__gCrWeb.getRegisteredApi('fill_test_api')."
        @"getFunction('getCanonicalActionForForm')(document.body.children[0])");
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
TEST_F(FillJsTest, GetAriaLabel) {
  LoadHtml(@"<input id='input' type='text' aria-label='the label'/>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
  NSString* expected_result = @"the label";
  EXPECT_NSEQ(result, expected_result);
}

// Tests if shouldAutocomplete returns valid result for
// autocomplete='one-time-code'.
TEST_F(FillJsTest, ShouldAutocompleteOneTimeCode) {
  LoadHtml(@"<input id='input' type='text' autocomplete='one-time-code'/>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('shouldAutocomplete')(document.getElementById('input'));");
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

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
  NSString* expected_result = @"Name";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-labelledby works: Complex case: multiple ids referenced.
TEST_F(FillJsTest, GetAriaLabelledByMulti) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-labelledby='billing name'/>"
            "</div>"
            "</body></html>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
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

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
  NSString* expected_result = @"Name";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that an invalid aria-labelledby reference gets ignored (as opposed to
// crashing, for example).
TEST_F(FillJsTest, GetAriaLabelledByInvalid) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-labelledby='div1 div2'/>"
            "</div>"
            "</body></html>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
  NSString* expected_result = @"";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that invalid aria-labelledby references fall back to aria-label.
TEST_F(FillJsTest, GetAriaLabelledByFallback) {
  LoadHtml(@"<html><body>"
            "<div id='billing'>Billing</div>"
            "<div>"
            "    <div id='name'>Name</div>"
            "    <input id='input' type='text' aria-label='valid' "
            "          aria-labelledby='div1 div2'/>"
            "</div>"
            "</body></html>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaLabel')(document.getElementById('input'));");
  NSString* expected_result = @"valid";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that aria-describedby works: Simple case: a single id referenced.
TEST_F(FillJsTest, GetAriaDescriptionSingle) {
  LoadHtml(@"<html><body>"
            "<input id='input' type='text' aria-describedby='div1'/>"
            "<div id='div1'>aria description</div>"
            "</body></html>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaDescription')(document.getElementById('input'));");
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

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaDescription')(document.getElementById('input'));");
  NSString* expected_result = @"aria description";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that invalid aria-describedby returns the empty string.
TEST_F(FillJsTest, GetAriaDescriptionInvalid) {
  LoadHtml(@"<html><body>"
            "<input id='input' type='text' aria-describedby='invalid'/>"
            "</body></html>");

  id result = ExecuteJavaScript(
      @"__gCrWeb.getRegisteredApi('fill_test_api')."
      @"getFunction('getAriaDescription')(document.getElementById('input'));");
  NSString* expected_result = @"";
  EXPECT_NSEQ(result, expected_result);
}

// Tests that getUniqueID from fill_test_api API returns the ID of an element
// from all JavaScript content worlds.
TEST_F(FillJsTest, GetUniqueIDInAllJavaScriptContentWorlds) {
  LoadHtml(@"<html><body>"
            "<form id='form'>"
            "<input id='input' type='text'></input>"
            "</form></body></html>");

  // Set IDs for form and input in the content world for Autofill features.
  ExecuteJavaScript(@"var form = document.getElementById('form');"
                     "__gCrWeb.getRegisteredApi('fill_test_api')."
                     "getFunction('setUniqueIDIfNeeded')(form);"
                     "var input = document.getElementById('input');"
                     "__gCrWeb.getRegisteredApi('fill_test_api')."
                     "getFunction('setUniqueIDIfNeeded')(input);");

  // Verify the ID retrieval in all content worlds.
  for (auto content_world : {web::ContentWorld::kIsolatedWorld,
                             web::ContentWorld::kPageContentWorld}) {
    SCOPED_TRACE(testing::Message()
                 << "content_world = " << static_cast<int>(content_world));
    // Check that the correct ID is returned for the form and input elements.
    // IDs should accessible from both content worlds.
    id form_id = GetUniqueID(@"form", content_world);
    EXPECT_NSEQ(form_id, @"1");

    id input_id = GetUniqueID(@"input", content_world);
    EXPECT_NSEQ(input_id, @"2");
  }
}

// Tests that getUniqueID from fill_test_api API returns the null ID when an
// invalid value is stored in the DOM.
TEST_F(FillJsTest, GetUniqueIDReturnsNotSetWhenInvalidIDInDOM) {
  LoadHtml(@"<html><body>"
            "<form id='form'/>"
            "</form></body></html>");

  // Set IDs for form and input in the content world for Autofill features.
  ExecuteJavaScript(@"var form = document.getElementById('form');"
                     "__gCrWeb.getRegisteredApi('fill_test_api')."
                     "getFunction('setUniqueIDIfNeeded')(form);");

  std::vector<NSString*> invalid_ids = {@"''", @"'word'", @"null",
                                        @"undefined"};

  for (NSString* invalid_id : invalid_ids) {
    SCOPED_TRACE(testing::Message() << "invalid_id = " << invalid_id);
    NSString* set_invalid_id_script = [NSString
        stringWithFormat:@"var form = document.getElementById('form');"
                          "form.setAttribute('__gCrUniqueID', %@);",
                         invalid_id];

    // Make the renderer ID invalid. The DOM is shared across content
    // worlds so it doesn't really matter which content world we use.
    ExecuteJavaScript(set_invalid_id_script);

    // Verify the ID retrieval in the isolated content world.
    // The ID should be non-zero because Autofill scripts run in this world and
    // will assign a valid ID.
    id isolated_form_id =
        GetUniqueID(@"form", web::ContentWorld::kIsolatedWorld);
    EXPECT_NSEQ(isolated_form_id, @"1");

    // Verify the ID retrieval in the page content world.
    // The ID should be zero because the DOM attribute is invalid and this is
    // not the Autofill content world.
    id page_form_id =
        GetUniqueID(@"form", web::ContentWorld::kPageContentWorld);
    EXPECT_NSEQ(page_form_id, @"0");
  }
}

// Tests stringify TS function.
TEST_F(FillJsTest, Stringify) {
  const auto kTestData = std::to_array<TestScriptAndExpectedValue>({
      // Stringify a string that contains various characters that must
      // be escaped.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')('a\\u000a\\t\\b\\\\\\\"Z')",
       @"\"a\\n\\t\\b\\\\\\\"Z\""},
      // Stringify a number.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')(77.7)",
       @"77.7"},
      // Stringify an array.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')(['a','b'])",
       @"[\"a\",\"b\"]"},
      // Stringify an object.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')({'a':'b','c':'d'})",
       @"{\"a\":\"b\",\"c\":\"d\"}"},
      // Stringify a hierarchy of objects and arrays.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')([{'a':['b','c'],'d':'e'},'f'])",
       @"[{\"a\":[\"b\",\"c\"],\"d\":\"e\"},\"f\"]"},
      // Stringify null.
      {@"__gCrWeb.getRegisteredApi('fill_test_api')."
       @"getFunction('stringify')(null)",
       @"null"},
      // Stringify an object with a toJSON function.
      {@"temp = [1,2];"
        "temp.toJSON = function (key) {return undefined};"
        "__gCrWeb.getRegisteredApi('fill_test_api').getFunction('stringify')("
        "temp)",
       @"[1,2]"},
      // Stringify an object with a toJSON property that is not a function.
      {@"temp = [1,2];"
        "temp.toJSON = 42;"
        "__gCrWeb.getRegisteredApi('fill_test_api').getFunction('stringify')("
        "temp)",
       @"[1,2]"},
      // Stringify an undefined object.
      {@"__gCrWeb.getRegisteredApi('fill_test_api').getFunction('stringify')("
       @"undefined)",
       @"undefined"},
  });

  for (const TestScriptAndExpectedValue& data : kTestData) {
    // Load a sample HTML page. As a side-effect, loading HTML via
    // `webController_` will also inject web_bundle.js.
    LoadHtml(@"<p>");
    id result = ExecuteJavaScript(data.test_script);
    EXPECT_NSEQ(data.expected_value, result)
        << " with input: " << base::SysNSStringToUTF8(data.test_script);
  }
}

// Tests that stringify works correctly even if JSON.stringify is overridden.
TEST_F(FillJsTest, StringifyJSONGlobalOverride) {
  LoadHtml(@"<p>");
  // Override JSON.stringify to return a random value.
  ExecuteJavaScript(@"JSON.stringify = function() { return 'broken'; }");

  id result = ExecuteJavaScript(@"__gCrWeb.getRegisteredApi('fill_test_api')."
                                @"getFunction('stringify')({'a':'b'})");
  EXPECT_NSEQ(result, @"{\"a\":\"b\"}");
}

// Tests that stringify ignores toJSON properties on Object.prototype and
// Array.prototype.
TEST_F(FillJsTest, StringifyPrototypeToJSON) {
  LoadHtml(@"<p>");
  // Object.prototype.toJSON override.
  ExecuteJavaScript(
      @"Object.prototype.toJSON = function() { return 'hacked object'; }");

  id obj_result =
      ExecuteJavaScript(@"__gCrWeb.getRegisteredApi('fill_test_api')."
                        @"getFunction('stringify')({'a':'b'})");
  EXPECT_NSEQ(obj_result, @"{\"a\":\"b\"}");

  // Array.prototype.toJSON override.
  ExecuteJavaScript(
      @"Array.prototype.toJSON = function() { return 'hacked array'; }");

  id arr_result =
      ExecuteJavaScript(@"__gCrWeb.getRegisteredApi('fill_test_api')."
                        @"getFunction('stringify')(['a','b'])");
  EXPECT_NSEQ(arr_result, @"[\"a\",\"b\"]");
}

// Tests that stringify restores the original toJSON method on the prototype
// after execution, even if it was overridden.
TEST_F(FillJsTest, StringifyRestoresPrototypeToJSON) {
  LoadHtml(@"<p>");

  ExecuteJavaScript(
      @"Array.prototype.toJSON = function() { return 'hacked array'; }");

  ExecuteJavaScript(@"__gCrWeb.getRegisteredApi('fill_test_api')."
                    @"getFunction('stringify')(['a','b'])");

  id result = ExecuteJavaScript(@"Array.prototype.toJSON.call([])");
  EXPECT_NSEQ(result, @"hacked array");
}

// Tests that stringify restores the original toJSON method on the object
// own property after execution.
TEST_F(FillJsTest, StringifyRestoresOwnToJSON) {
  LoadHtml(@"<p>");

  ExecuteJavaScript(@"var obj = { 'a': 'b' };"
                    @"obj.toJSON = function() { return 'own toJSON'; };"
                    @"__gCrWeb.getRegisteredApi('fill_test_api')."
                    @"getFunction('stringify')(obj)");

  id result = ExecuteJavaScript(@"obj.toJSON()");
  EXPECT_NSEQ(result, @"own toJSON");
}

}  // namespace autofill
