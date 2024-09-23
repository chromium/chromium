// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/values.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

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
        error_type_(TranslateErrors::NONE),
        ready_time_(0),
        load_time_(0),
        translation_time_(0),
        on_script_ready_called_(false),
        on_translate_complete_called_(false) {
    fake_web_state_->SetBrowserState(fake_browser_state_.get());
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web::ContentWorld content_world =
        TranslateJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    fake_web_state_->SetWebFramesManager(content_world,
                                         std::move(frames_manager));
    TranslateController::CreateForWebState(fake_web_state_.get());
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

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<web::FakeBrowserState> fake_browser_state_;
  std::unique_ptr<web::FakeWebFrame> fake_main_frame_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
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

}  // namespace translate
