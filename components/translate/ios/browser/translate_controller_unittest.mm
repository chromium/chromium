// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <memory>

#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager_factory.h"
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

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

#pragma mark - FakeJSTranslateWebFrameManager

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

class FakeJSTranslateWebFrameManager : public JSTranslateWebFrameManager {
 public:
  FakeJSTranslateWebFrameManager(web::WebFrame* web_frame)
      : JSTranslateWebFrameManager(web_frame) {}
  ~FakeJSTranslateWebFrameManager() override = default;

  void HandleTranslateResponse(const std::string& url,
                               int request_id,
                               int response_code,
                               const std::string status_text,
                               const std::string& response_url,
                               const std::string& response_text) override {
    last_handle_response_params_ =
        std::make_unique<HandleTranslateResponseParams>(
            url, request_id, response_code, status_text, response_url,
            response_text);
  }

  HandleTranslateResponseParams* GetLastHandleResponseParams() {
    return last_handle_response_params_.get();
  }

 private:
  std::unique_ptr<HandleTranslateResponseParams> last_handle_response_params_;
};

class FakeJSTranslateWebFrameManagerFactory
    : public JSTranslateWebFrameManagerFactory {
 public:
  FakeJSTranslateWebFrameManagerFactory() {}
  ~FakeJSTranslateWebFrameManagerFactory() {}

  static FakeJSTranslateWebFrameManagerFactory* GetInstance() {
    static base::NoDestructor<FakeJSTranslateWebFrameManagerFactory> instance;
    return instance.get();
  }

  FakeJSTranslateWebFrameManager* FromWebFrame(
      web::WebFrame* web_frame) override {
    if (managers_.find(web_frame->GetFrameId()) == managers_.end()) {
      managers_[web_frame->GetFrameId()] =
          std::make_unique<FakeJSTranslateWebFrameManager>(web_frame);
    }
    return managers_[web_frame->GetFrameId()].get();
  }

  void CreateForWebFrame(web::WebFrame* web_frame) override {
    // no-op, managers are created lazily in FromWebState
  }

 private:
  std::map<std::string, std::unique_ptr<FakeJSTranslateWebFrameManager>>
      managers_;
};

}  // namespace

namespace translate {

class TranslateControllerTest : public PlatformTest,
                                public TranslateController::Observer {
 protected:
  TranslateControllerTest()
      : task_environment_(web::WebTaskEnvironment::Options::IO_MAINLOOP),
        fake_web_state_(std::make_unique<web::FakeWebState>()),
        fake_browser_state_(std::make_unique<web::FakeBrowserState>()),
        fake_main_frame_(web::FakeWebFrame::Create(/*frame_id=*/"",
                                                   /*is_main_frame=*/true,
                                                   GURL())),
        error_type_(TranslateErrors::NONE),
        ready_time_(0),
        load_time_(0),
        translation_time_(0),
        on_script_ready_called_(false),
        on_translate_complete_called_(false) {
    fake_web_state_->SetBrowserState(fake_browser_state_.get());
    TranslateController::CreateForWebState(fake_web_state_.get(),
                                           &fake_translate_factory_);
    TranslateController::FromWebState(fake_web_state_.get())
        ->set_observer(this);
  }

  // TranslateController::Observer methods.
  void OnTranslateScriptReady(TranslateErrors error_type,
                              double load_time,
                              double ready_time) override {
    on_script_ready_called_ = true;
    error_type_ = error_type;
    load_time_ = load_time;
    ready_time_ = ready_time;
  }

  void OnTranslateComplete(TranslateErrors error_type,
                           const std::string& source_language,
                           double translation_time) override {
    on_translate_complete_called_ = true;
    error_type_ = error_type;
    source_language_ = source_language;
    translation_time_ = translation_time;
  }

  TranslateController* translate_controller() {
    return TranslateController::FromWebState(fake_web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<web::FakeBrowserState> fake_browser_state_;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame_;
  FakeJSTranslateWebFrameManagerFactory fake_translate_factory_;
  TranslateErrors error_type_;
  double ready_time_;
  double load_time_;
  std::string source_language_;
  double translation_time_;
  bool on_script_ready_called_;
  bool on_translate_complete_called_;
};

// Tests that OnTranslateScriptReady() is called when a timeout message is
// received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateScriptReadyTimeoutCalled) {
  base::Value::Dict command;
  command.Set("command", "ready");
  command.Set("errorCode",
              static_cast<double>(TranslateErrors::TRANSLATION_TIMEOUT));
  command.Set("loadTime", .0);
  command.Set("readyTime", .0);
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));
  EXPECT_TRUE(on_script_ready_called_);
  EXPECT_FALSE(on_translate_complete_called_);
  EXPECT_FALSE(error_type_ == TranslateErrors::NONE);
}

// Tests that OnTranslateScriptReady() is called with the right parameters when
// a `ready` message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateScriptReadyCalled) {
  // Arbitrary values.
  double some_load_time = 23.1;
  double some_ready_time = 12.2;

  base::Value::Dict command;
  command.Set("command", "ready");
  command.Set("errorCode", static_cast<double>(TranslateErrors::NONE));
  command.Set("loadTime", some_load_time);
  command.Set("readyTime", some_ready_time);
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));
  EXPECT_TRUE(on_script_ready_called_);
  EXPECT_FALSE(on_translate_complete_called_);
  EXPECT_TRUE(error_type_ == TranslateErrors::NONE);
  EXPECT_EQ(some_load_time, load_time_);
  EXPECT_EQ(some_ready_time, ready_time_);
}

// Tests that OnTranslateComplete() is called with the right parameters when a
// `status` message is received from the JS side.
TEST_F(TranslateControllerTest, TranslationSuccess) {
  // Arbitrary values.
  std::string some_source_language("en");
  double some_translation_time = 12.9;

  base::Value::Dict command;
  command.Set("command", "status");
  command.Set("errorCode", static_cast<double>(TranslateErrors::NONE));
  command.Set("pageSourceLanguage", some_source_language);
  command.Set("translationTime", some_translation_time);
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_TRUE(error_type_ == TranslateErrors::NONE);
  EXPECT_EQ(some_source_language, source_language_);
  EXPECT_EQ(some_translation_time, translation_time_);
}

// Tests that OnTranslateComplete() is called with the right parameters when a
// `status` message is received from the JS side.
TEST_F(TranslateControllerTest, TranslationFailure) {
  base::Value::Dict command;
  command.Set("command", "status");
  command.Set("errorCode",
              static_cast<double>(TranslateErrors::INITIALIZATION_ERROR));
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));
  EXPECT_FALSE(on_script_ready_called_);
  EXPECT_TRUE(on_translate_complete_called_);
  EXPECT_FALSE(error_type_ == TranslateErrors::NONE);
}

// Tests that OnTranslateSendRequest() is called with the right parameters
// when a `sendrequest` message is received from the JS side.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithValidCommand) {
  fake_web_state_->OnWebFrameDidBecomeAvailable(fake_main_frame_.get());

  base::Value::Dict command;
  command.Set("command", "sendrequest");
  command.Set("method", "POST");
  command.Set("url", "https://translate.googleapis.com/translate?key=abcd");
  command.Set("body", "helloworld");
  command.Set("requestID", .0);
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));

  __block HandleTranslateResponseParams* last_params = nullptr;
  FakeJSTranslateWebFrameManager* fake_translate_manager =
      fake_translate_factory_.FromWebFrame(fake_main_frame_.get());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    task_environment_.RunUntilIdle();
    last_params = fake_translate_manager->GetLastHandleResponseParams();
    return last_params != nullptr;
  }));

  EXPECT_EQ("https://translate.googleapis.com/translate?key=abcd",
            last_params->URL);
  EXPECT_EQ(0, last_params->request_ID);
  EXPECT_EQ(net::HttpStatusCode::HTTP_BAD_REQUEST, last_params->response_code);
  EXPECT_EQ("", last_params->status_text);
  EXPECT_EQ("https://translate.googleapis.com/translate?key=abcd",
            last_params->response_URL);
  EXPECT_EQ("", last_params->response_text);
}

// Tests that OnTranslateSendRequest() rejects a bad url contained in the
// `sendrequest` message received from Javascript.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithBadURL) {
  base::Value::Dict command;
  command.Set("command", "sendrequest");
  command.Set("method", "POST");
  command.Set("url", "https://badurl.example.com");
  command.Set("body", "helloworld");
  command.Set("requestID", .0);
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));
  task_environment_.RunUntilIdle();
  HandleTranslateResponseParams* last_params =
      fake_translate_factory_.FromWebFrame(fake_main_frame_.get())
          ->GetLastHandleResponseParams();
  ASSERT_FALSE(last_params);
}

// Tests that OnTranslateSendRequest() called with a bad method will eventually
// cause the request to fail.
TEST_F(TranslateControllerTest, OnTranslateSendRequestWithBadMethod) {
  fake_web_state_->OnWebFrameDidBecomeAvailable(fake_main_frame_.get());

  base::Value::Dict command;
  command.Set("command", "sendrequest");
  command.Set("method", "POST\r\nHost: other.example.com");
  command.Set("url", "https://translate.googleapis.com/translate?key=abcd");
  command.Set("body", "helloworld");
  command.Set("requestID", .0);

  // The command will be accepted, but a bad method should cause the request to
  // fail shortly thereafter.
  translate_controller()->OnJavascriptCommandReceived(
      base::Value::Dict(std::move(command)));

  __block HandleTranslateResponseParams* last_params = nullptr;
  FakeJSTranslateWebFrameManager* fake_translate_manager =
      fake_translate_factory_.FromWebFrame(fake_main_frame_.get());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    task_environment_.RunUntilIdle();
    last_params = fake_translate_manager->GetLastHandleResponseParams();
    return last_params != nullptr;
  }));

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
