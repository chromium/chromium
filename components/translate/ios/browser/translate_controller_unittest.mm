// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <memory>

#include "base/values.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - FakeJSTranslateManager

namespace {

struct HandleTranslateResponseParams {
  HandleTranslateResponseParams(const std::string& URL,
                                int request_ID,
                                int response_code,
                                const std::string& status_text,
                                const std::string& response_URL,
                                const std::string& response_text)
      : URL(URL),
        request_ID(request_ID),
        response_code(response_code),
        status_text(status_text),
        response_URL(response_URL),
        response_text(response_text) {}
  ~HandleTranslateResponseParams() {}
  std::string URL;
  int request_ID;
  int response_code;
  std::string status_text;
  std::string response_URL;
  std::string response_text;
};

}  // namespace

// Fake translate manager to store parameters passed to
// |handleTranslateResponseWithURL:...|
@interface FakeJSTranslateManager : JsTranslateManager

@property(readonly) HandleTranslateResponseParams* lastHandleResponseParams;

@end

@implementation FakeJSTranslateManager {
  std::unique_ptr<HandleTranslateResponseParams> _lastHandleResponseParams;
}

- (HandleTranslateResponseParams*)lastHandleResponseParams {
  return _lastHandleResponseParams.get();
}

- (void)handleTranslateResponseWithURL:(const std::string&)URL
                             requestID:(int)requestID
                          responseCode:(int)responseCode
                            statusText:(const std::string&)statusText
                           responseURL:(const std::string&)responseURL
                          responseText:(const std::string&)responseText {
  _lastHandleResponseParams = std::make_unique<HandleTranslateResponseParams>(
      URL, requestID, responseCode, statusText, responseURL, responseText);
}

@end

namespace translate {

class TranslateControllerTest : public PlatformTest,
                                public TranslateController::Observer {
 protected:
  TranslateControllerTest()
      : fake_web_state_(std::make_unique<web::FakeWebState>()),
        fake_browser_state_(std::make_unique<web::FakeBrowserState>()),
        fake_main_frame_(web::FakeWebFrame::Create(/*frame_id=*/"",
                                                   /*is_main_frame=*/true,
                                                   GURL())),
        fake_iframe_(web::FakeWebFrame::Create(/*frame_id=*/"",
                                               /*is_main_frame=*/false,
                                               GURL())),
        error_type_(TranslateErrors::Type::NONE),
        ready_time_(0),
        load_time_(0),
        translation_time_(0),
        on_script_ready_called_(false),
        on_translate_complete_called_(false) {
    fake_web_state_->SetBrowserState(fake_browser_state_.get());
    fake_js_translate_manager_ =
        [[FakeJSTranslateManager alloc] initWithWebState:fake_web_state_.get()];
    translate_controller_ = std::make_unique<TranslateController>(
        fake_web_state_.get(), fake_js_translate_manager_);
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
                           const std::string& source_language,
                           double translation_time) override {
    on_translate_complete_called_ = true;
    error_type_ = error_type;
    source_language_ = source_language;
    translation_time_ = translation_time;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<web::FakeBrowserState> fake_browser_state_;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame_;
  std::unique_ptr<web::FakeWebFrame> fake_iframe_;
  FakeJSTranslateManager* fake_js_translate_manager_;
  std::unique_ptr<TranslateController> translate_controller_;
  TranslateErrors::Type error_type_;
  double ready_time_;
  double load_time_;
  std::string source_language_;
  double translation_time_;
  bool on_script_ready_called_;
  bool on_translate_complete_called_;
};

// Tests that OnJavascriptCommandReceived() returns false to malformed commands.
TEST_F(TranslateControllerTest, OnJavascriptCommandReceived) {
  base::DictionaryValue malformed_command;
  EXPECT_FALSE(translate_controller_->OnJavascriptCommandReceived(
      malformed_command, GURL("http://google.com"), /*interacting*/ false,
      fake_main_frame_.get()));
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
      fake_iframe_.get()));
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
      fake_main_frame_.get()));
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
      fake_main_frame_.get()));
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
  std::string some_source_language("en");
  double some_translation_time = 12.9;

  base::DictionaryValue command;
  command.SetString("command", "translate.status");
  command.SetDouble("errorCode", TranslateErrors::NONE);
  command.SetString("pageSourceLanguage", some_source_language);
  command.SetDouble("translationTime", some_translation_time);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting*/ false,
      fake_main_frame_.get()));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_TRUE(error_type_ == TranslateErrors::NONE);
  EXPECT_EQ(some_source_language, source_language_);
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
      fake_main_frame_.get()));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_FALSE(error_type_ == TranslateErrors::NONE);
}

// Tests that OnTranslateLoadJavaScript() is called with the right parameters
// when a |translate.loadjavascript| message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateLoadJavascript) {
  base::DictionaryValue command;
  command.SetString("command", "translate.loadjavascript");
  command.SetString("url", "https://translate.googleapis.com/javascript.js");
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      fake_main_frame_.get()));
}

// Tests that OnTranslateSendRequest() is called with the right parameters
// when a |translate.sendrequest| message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithValidCommand) {
  base::DictionaryValue command;
  command.SetString("command", "translate.sendrequest");
  command.SetString("method", "POST");
  command.SetString("url",
                    "https://translate.googleapis.com/translate?key=abcd");
  command.SetString("body", "helloworld");
  command.SetDouble("requestID", 0);
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      fake_main_frame_.get()));
}

// Tests that OnTranslateSendRequest() rejects a bad url contained in the
// |translate.sendrequest| message received from Javascript.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithBadURL) {
  base::DictionaryValue command;
  command.SetString("command", "translate.sendrequest");
  command.SetString("method", "POST");
  command.SetString("url", "https://badurl.example.com");
  command.SetString("body", "helloworld");
  command.SetDouble("requestID", 0);
  EXPECT_FALSE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      fake_main_frame_.get()));
}

// Tests that OnTranslateSendRequest() called with a bad method will eventually
// cause the request to fail.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithBadMethod) {
  base::DictionaryValue command;
  command.SetString("command", "translate.sendrequest");
  command.SetString("method", "POST\r\nHost: other.example.com");
  command.SetString("url",
                    "https://translate.googleapis.com/translate?key=abcd");
  command.SetString("body", "helloworld");
  command.SetDouble("requestID", 0);

  // The command will be accepted, but a bad method should cause the request to
  // fail shortly thereafter.
  EXPECT_TRUE(translate_controller_->OnJavascriptCommandReceived(
      command, GURL("http://google.com"), /*interacting=*/false,
      fake_main_frame_.get()));
  task_environment_.RunUntilIdle();

  HandleTranslateResponseParams* last_params =
      fake_js_translate_manager_.lastHandleResponseParams;
  ASSERT_TRUE(last_params);
  EXPECT_EQ("https://translate.googleapis.com/translate?key=abcd",
            last_params->URL);
  EXPECT_EQ(0, last_params->request_ID);
  EXPECT_EQ(net::HttpStatusCode::HTTP_BAD_REQUEST, last_params->response_code);
  EXPECT_EQ("", last_params->status_text);
  EXPECT_EQ("https://translate.googleapis.com/translate?key=abcd",
            last_params->response_URL);
  EXPECT_EQ("", last_params->response_text);
}

}  // namespace translate
