// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/language_detection_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MockJsLanguageDetectionManager : JsLanguageDetectionManager
@end

@implementation MockJsLanguageDetectionManager
- (void)retrieveBufferedTextContent:
        (const language_detection::BufferedTextCallback&)callback {
  callback.Run(base::UTF8ToUTF16("Some content"));
}
@end

namespace translate {

class LanguageDetectionControllerTest
    : public PlatformTest,
      public language::IOSLanguageDetectionTabHelper::Observer {
 protected:
  LanguageDetectionControllerTest()
      : fake_main_frame_(/*frame_id=*/"", /*is_main_frame=*/true, GURL()),
        details_(nullptr) {
    prefs_.registry()->RegisterBooleanPref(prefs::kOfferTranslateEnabled, true);

    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_,
        /*url_language_histogram=*/nullptr);
    auto* language_detection_tab_helper =
        language::IOSLanguageDetectionTabHelper::FromWebState(&web_state_);
    language_detection_tab_helper->AddObserver(this);

    MockJsLanguageDetectionManager* js_manager =
        [[MockJsLanguageDetectionManager alloc] init];
    controller_ = std::make_unique<LanguageDetectionController>(
        &web_state_, js_manager, &prefs_);
  }

  ~LanguageDetectionControllerTest() override {
    auto* language_detection_tab_helper =
        language::IOSLanguageDetectionTabHelper::FromWebState(&web_state_);
    language_detection_tab_helper->RemoveObserver(this);
  }

  web::TestWebState& web_state() { return web_state_; }
  LanguageDetectionController* controller() { return controller_.get(); }
  LanguageDetectionDetails* details() { return details_.get(); }

 protected:
  web::FakeWebFrame fake_main_frame_;

 private:
  TestingPrefServiceSimple prefs_;
  web::TestWebState web_state_;
  std::unique_ptr<LanguageDetectionController> controller_;
  std::unique_ptr<LanguageDetectionDetails> details_;

  void OnLanguageDetermined(const LanguageDetectionDetails& details) override {
    details_ = std::make_unique<LanguageDetectionDetails>(details);
  }

  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override {
    // No-op.
  }
};

// Tests that OnTextCaptured() correctly handles messages from the JS side and
// informs the driver.
TEST_F(LanguageDetectionControllerTest, OnTextCaptured) {
  const std::string kRootLanguage("en");
  const std::string kContentLanguage("fr");
  const std::string kUndefined("und");

  base::DictionaryValue command;
  command.SetString("command", "languageDetection.textCaptured");
  command.SetBoolean("translationAllowed", true);
  command.SetInteger("captureTextTime", 10);
  command.SetString("htmlLang", kRootLanguage);
  command.SetString("httpContentLanguage", kContentLanguage);
  controller()->OnTextCaptured(command, GURL("http://google.com"),
                               /*interacting=*/false, &fake_main_frame_);

  const LanguageDetectionDetails* const details = this->details();
  EXPECT_NE(nullptr, details);
  EXPECT_EQ(kRootLanguage, details->html_root_language);
  EXPECT_EQ(kContentLanguage, details->content_language);
  EXPECT_FALSE(details->is_cld_reliable);
  EXPECT_EQ(kUndefined, details->cld_language);
}

// Tests that Content-Language response header is used if httpContentLanguage
// message value is empty.
TEST_F(LanguageDetectionControllerTest, MissingHttpContentLanguage) {
  // Pass content-language header to LanguageDetectionController.
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(""));
  headers->AddHeader("Content-Language: fr, en-CA");
  web::FakeNavigationContext context;
  context.SetResponseHeaders(headers);
  web_state().OnNavigationFinished(&context);

  base::DictionaryValue command;
  command.SetString("command", "languageDetection.textCaptured");
  command.SetBoolean("translationAllowed", true);
  command.SetInteger("captureTextTime", 10);
  command.SetString("htmlLang", "");
  command.SetString("httpContentLanguage", "");
  controller()->OnTextCaptured(command, GURL("http://google.com"),
                               /*interacting=*/false, &fake_main_frame_);

  const LanguageDetectionDetails* const details = this->details();
  EXPECT_NE(nullptr, details);
  EXPECT_EQ("fr", details->content_language);
}

}  // namespace translate
