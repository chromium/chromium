// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <memory>

#include "base/values.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

class TranslateControllerTest : public PlatformTest,
                                public TranslateController::Observer {
 protected:
  TranslateControllerTest()
      : test_web_state_(new web::TestWebState),
        test_browser_state_(new web::TestBrowserState),
        fake_main_frame_(/*frame_id=*/"", /*is_main_frame=*/true, GURL()),
        fake_iframe_(/*frame_id=*/"", /*is_main_frame=*/false, GURL()),
        error_type_(TranslateErrors::Type::NONE),
        ready_time_(0),
        load_time_(0),
        translation_time_(0),
        on_script_ready_called_(false),
        on_translate_complete_called_(false) {
    test_web_state_->SetBrowserState(test_browser_state_.get());
    mock_js_translate_manager_ =
        [OCMockObject niceMockForClass:[JsTranslateManager class]];
    translate_controller_ = std::make_unique<TranslateController>(
        test_web_state_.get(), mock_js_translate_manager_);
    translate_controller_->set_observer(this);
  }

  // TranslateController::Observer methods.
  void OnTranslateScriptReady(TranslateErrors::Type error_type,
                              double load_time,
                              double ready_time) override {
    on_script_ready_called_ = true;
    error_type_ = error_type;
    load_time_ = load_time;
    ready_time_ = ready_time;
  }

  void OnTranslateComplete(TranslateErrors::Type error_type,
                           const std::string& original_language,
                           double translation_time) override {
    on_translate_complete_called_ = true;
    error_type_ = error_type;
    original_language_ = original_language;
    translation_time_ = translation_time;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::TestWebState> test_web_state_;
  std::unique_ptr<web::TestBrowserState> test_browser_state_;
  web::FakeWebFrame fake_main_frame_;
  web::FakeWebFrame fake_iframe_;
  id mock_js_translate_manager_;
  std::unique_ptr<TranslateController> translate_controller_;
  TranslateErrors::Type error_type_;
  double ready_time_;
  double load_time_;
  std::string original_language_;
  double translation_time_;
  bool on_script_ready_called_;
  bool on_translate_complete_called_;
};

// Tests that OnJavascriptCommandReceived() returns false to malformed commands.
TEST_F(TranslateControllerTest, OnJavascriptCommandReceived) {
  base::DictionaryValue malformed_command;
  EXPECT_FALSE(translate_controller_->OnJavascriptCommandReceived(
      malformed_command, GURL("http://google.com"), /*interacting*/ false,
      &fake_main_frame_));
}

// Tests that OnJavascriptCommandReceived() returns false to iframe commands.
TEST_F(TranslateControllerTest, OnIFrameJavascriptCommandReceived) {
  base::DictionaryValue command;
  command.SetString("command", "translate.ready");
  command.SetDouble("errorCode", TranslateErrors::TRANSLATION_TIMEOUT);
  command.SetDouble("loadTime", .0);
  command.SetDouble("readyTime", .0);
  EXPECT_FALSE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      &fake_iframe_));
}

// Tests that OnTranslateScriptReady() is called when a timeout message is
// received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateScriptReadyTimeoutCalled) {
  base::DictionaryValue command;
  command.SetString("command", "translate.ready");
  command.SetDouble("errorCode", TranslateErrors::TRANSLATION_TIMEOUT);
  command.SetDouble("loadTime", .0);
  command.SetDouble("readyTime", .0);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      &fake_main_frame_));
  EXPECT_TRUE(on_script_ready_called_);
  EXPECT_FALSE(on_translate_complete_called_);
  EXPECT_FALSE(error_type_ == TranslateErrors::NONE);
}

// Tests that OnTranslateScriptReady() is called with the right parameters when
// a |translate.ready| message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateScriptReadyCalled) {
  // Arbitrary values.
  double some_load_time = 23.1;
  double some_ready_time = 12.2;

  base::DictionaryValue command;
  command.SetString("command", "translate.ready");
  command.SetDouble("errorCode", TranslateErrors::NONE);
  command.SetDouble("loadTime", some_load_time);
  command.SetDouble("readyTime", some_ready_time);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      &fake_main_frame_));
  EXPECT_TRUE(on_script_ready_called_);
  EXPECT_FALSE(on_translate_complete_called_);
  EXPECT_TRUE(error_type_ == TranslateErrors::NONE);
  EXPECT_EQ(some_load_time, load_time_);
  EXPECT_EQ(some_ready_time, ready_time_);
}

// Tests that OnTranslateComplete() is called with the right parameters when a
// |translate.status| message is received from the JS side.
TEST_F(TranslateControllerTest, TranslationSuccess) {
  // Arbitrary values.
  std::string some_original_language("en");
  double some_translation_time = 12.9;

  base::DictionaryValue command;
  command.SetString("command", "translate.status");
  command.SetDouble("errorCode", TranslateErrors::NONE);
  command.SetString("originalPageLanguage", some_original_language);
  command.SetDouble("translationTime", some_translation_time);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      &fake_main_frame_));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_TRUE(error_type_ == TranslateErrors::NONE);
  EXPECT_EQ(some_original_language, original_language_);
  EXPECT_EQ(some_translation_time, translation_time_);
}

// Tests that OnTranslateComplete() is called with the right parameters when a
// |translate.status| message is received from the JS side.
TEST_F(TranslateControllerTest, TranslationFailure) {
  base::DictionaryValue command;
  command.SetString("command", "translate.status");
  command.SetDouble("errorCode", TranslateErrors::INITIALIZATION_ERROR);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      &fake_main_frame_));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_FALSE(error_type_ == TranslateErrors::NONE);
}

// Tests that OnTranslateLoadJavaScript() is called with the right paramters
// when a |translate.loadjavascript| message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateLoadJavascript) {
  base::DictionaryValue command;
  command.SetString("command", "translate.loadjavascript");
  command.SetString("url", "https://translate.googleapis.com/javascript.js");
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      &fake_main_frame_));
}

// Tests that OnTranslateSendRequest() is called with the right paramters
// when a |translate.sendrequest| message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateSendRequest) {
  base::DictionaryValue command;
  command.SetString("command", "translate.sendrequest");
  command.SetString("method", "POST");
  command.SetString("url",
                    "https://translate.googleapis.com/translate?key=abcd");
  command.SetString("body", "helloworld");
  command.SetDouble("requestID", 0);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      &fake_main_frame_));
}

}  // namespace translate
