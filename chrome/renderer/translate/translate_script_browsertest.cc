// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/grit/components_resources.h"
#include "components/translate/core/common/translate_errors.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

using blink::WebScriptSource;

namespace {

// JavaScript code to set runtime test flags.
const char kThrowInitializationError[] = "throwInitializationError = true";
const char kThrowUnexpectedScriptError[] = "throwUnexpectedScriptError = true";
const char kCallbackReturnBooleanError[] = "callbackReturnBooleanError = true";
const char kCallbackReturnNumberError[] = "callbackReturnNumberError = true";
const char kSetCallbackErrorCode[] = "callbackErrorCode = ";

// JavaScript code to check if any error happens.
const char kError[] = "cr.googleTranslate.error";

// JavaScript code to get error code.
const char kErrorCode[] = "cr.googleTranslate.errorCode";

// JavaScript code to check if the library is ready.
const char kLibReady[] = "cr.googleTranslate.libReady";

// JavaScript code to perform translation.
const char kTranslate[] = "cr.googleTranslate.translate('auto', 'en')";

// JavaScript code to mimic element.js provided by a translate server.
const char kElementJs[] =
    "serverParams = '';"
    "gtTimeInfo = {};"
    "translateApiKey = '';"
    "google = {};"
    "google.translate = {};"
    "google.translate.TranslateService = function() {"
    "  if (window['throwInitializationError']) {"
    "    throw 'API initialization error';"
    "  }"
    "  return {"
    "    isAvailable: function() { return true; },"
    "    restore: function() {},"
    "    translatePage: function(originalLang, targetLang, cb) {"
    "      if (window['throwUnexpectedScriptError']) {"
    "        throw 'all your base are belong to us';"
    "      }"
    "      if (window['callbackReturnBooleanError']) {"
    "        cb(0, false, true);"
    "      }"
    "      if (window['callbackReturnNumberError']) {"
    "        cb(0, false, callbackErrorCode);"
    "      }"
    "    }"
    "  };"
    "};"
    "cr.googleTranslate.onTranslateElementLoad();";

std::string GenerateSetCallbackErrorCodeScript(int code) {
  return base::StringPrintf("%s%d", kSetCallbackErrorCode, code);
}

}  // namespace

// This class is for testing resource/translate.js works and reports errors
// correctly.
class TranslateScriptBrowserTest : public ChromeRenderViewTest {
 public:
  TranslateScriptBrowserTest() {}

 protected:
  void InjectElementLibrary() {
    std::string script =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_TRANSLATE_JS);
    script += kElementJs;
    ExecuteScript(script);
  }

  void ExecuteScript(const std::string& script) {
    WebScriptSource source =
        WebScriptSource(blink::WebString::FromASCII(script));
    GetMainFrame()->ExecuteScript(source);
  }

  bool GetError() {
    return ExecuteScriptAndGetBoolResult(kError);
  }

  double GetErrorCode() {
    return ExecuteScriptAndGetNumberResult(kErrorCode);
  }

  bool IsLibReady() {
    return ExecuteScriptAndGetBoolResult(kLibReady);
  }

 private:
  double ExecuteScriptAndGetNumberResult(const std::string& script) {
    WebScriptSource source =
        WebScriptSource(blink::WebString::FromASCII(script));
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    v8::Local<v8::Value> result =
        GetMainFrame()->ExecuteScriptAndReturnValue(source);
    if (result.IsEmpty() || !result->IsNumber()) {
      NOTREACHED();
      // TODO(toyoshim): Return NaN here and the real implementation in
      // TranslateHelper::ExecuteScriptAndGetDoubleResult().
      return 0.0;
    }
    return result.As<v8::Number>()->Value();
  }

  bool ExecuteScriptAndGetBoolResult(const std::string& script) {
    WebScriptSource source =
        WebScriptSource(blink::WebString::FromASCII(script));
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    v8::Local<v8::Value> result =
        GetMainFrame()->ExecuteScriptAndReturnValue(source);
    if (result.IsEmpty() || !result->IsBoolean()) {
      NOTREACHED();
      return false;
    }
    return result.As<v8::Boolean>()->Value();
  }

  DISALLOW_COPY_AND_ASSIGN(TranslateScriptBrowserTest);
};

// Test if onTranslateElementLoad() succeeds to initialize the element library.
TEST_F(TranslateScriptBrowserTest, ElementLoadSuccess) {
  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());
}

// Test if onTranslateElementLoad() fails to initialize the element library and
// report the right error code.
TEST_F(TranslateScriptBrowserTest, ElementLoadFailure) {
  ExecuteScript(kThrowInitializationError);

  InjectElementLibrary();
  EXPECT_FALSE(IsLibReady());
  EXPECT_TRUE(GetError());
  EXPECT_EQ(translate::TranslateErrors::INITIALIZATION_ERROR, GetErrorCode());
}

// Test if cr.googleTranslate.translate() works.
TEST_F(TranslateScriptBrowserTest, TranslateSuccess) {
  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());

  ExecuteScript(kTranslate);

  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());
}

// Test if cr.googleTranslate.translate() handles library exception correctly.
TEST_F(TranslateScriptBrowserTest, TranslateFail) {
  ExecuteScript(kThrowUnexpectedScriptError);

  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());

  ExecuteScript(kTranslate);

  EXPECT_TRUE(GetError());
  EXPECT_EQ(translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR,
            GetErrorCode());
}

// Test if onTranslateProgress callback handles boolean type error correctly.
// Remove this test once server side changes the API to return a number.
TEST_F(TranslateScriptBrowserTest, CallbackGetBooleanError) {
  ExecuteScript(kCallbackReturnBooleanError);

  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());

  ExecuteScript(kTranslate);

  EXPECT_TRUE(GetError());
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR, GetErrorCode());
}

// Test if onTranslateProgress callback handles number type error correctly and
// converts it to the proper error code.
TEST_F(TranslateScriptBrowserTest, CallbackGetNumberError1) {
  ExecuteScript(kCallbackReturnNumberError);
  ExecuteScript(GenerateSetCallbackErrorCodeScript(1));

  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());

  ExecuteScript(kTranslate);

  EXPECT_TRUE(GetError());
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR, GetErrorCode());
}

// Test if onTranslateProgress callback handles number type error correctly and
// converts it to the proper error code.
TEST_F(TranslateScriptBrowserTest, CallbackGetNumberError2) {
  ExecuteScript(kCallbackReturnNumberError);
  ExecuteScript(GenerateSetCallbackErrorCodeScript(2));

  InjectElementLibrary();
  EXPECT_TRUE(IsLibReady());
  EXPECT_FALSE(GetError());
  EXPECT_EQ(translate::TranslateErrors::NONE, GetErrorCode());

  ExecuteScript(kTranslate);

  EXPECT_TRUE(GetError());
  EXPECT_EQ(translate::TranslateErrors::UNSUPPORTED_LANGUAGE, GetErrorCode());
}

// TODO(toyoshim): Add test for onLoadJavaScript.
