// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/ios_translate_driver.h"

#include "components/language/core/browser/language_model.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

using testing::_;

namespace {
class MockLanguageModel : public language::LanguageModel {
  std::vector<language::LanguageModel::LanguageDetails> GetLanguages()
      override {
    return {language::LanguageModel::LanguageDetails("en", 1.0)};
  }
};
}  // namespace

namespace translate {

class IOSTranslateDriverTest : public PlatformTest {
 protected:
  IOSTranslateDriverTest()
      : fake_browser_state_(std::make_unique<web::FakeBrowserState>()),
        fake_web_state_(std::make_unique<web::FakeWebState>()) {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        TranslateJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    fake_web_state_->SetWebFramesManager(content_world,
                                         std::move(web_frames_manager));
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        fake_web_state_.get(), nullptr, nullptr, pref_service_.get());

    driver_ =
        std::make_unique<IOSTranslateDriver>(fake_web_state_.get(), nullptr);
    mock_translate_client_ =
        std::make_unique<::testing::NiceMock<testing::MockTranslateClient>>(
            driver_.get(), pref_service_.get());

    translate_manager_ = std::make_unique<TranslateManager>(
        mock_translate_client_.get(), &mock_translate_ranker_,
        &mock_language_model_);
    driver_->Initialize(nullptr, translate_manager_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<web::FakeBrowserState> fake_browser_state_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<IOSTranslateDriver> driver_;
  std::unique_ptr<::testing::NiceMock<testing::MockTranslateClient>>
      mock_translate_client_;
  MockLanguageModel mock_language_model_;
  testing::MockTranslateRanker mock_translate_ranker_;
  std::unique_ptr<TranslateManager> translate_manager_;
};

// Test that if there is a timeout, only the error is shown.
TEST_F(IOSTranslateDriverTest, TestTimeout) {
  driver_->PrepareToTranslatePage(0, "en", "fr", true);

  EXPECT_CALL(*mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, _, _,
                              TranslateErrors::TRANSLATION_TIMEOUT, _))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(*mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, _, _,
                              TranslateErrors::NONE, _))
      .Times(0);
  task_environment_.AdvanceClock(base::Seconds(10));
  task_environment_.RunUntilIdle();
  driver_->OnTranslateComplete(TranslateErrors::NONE, "en", 1.0);
}

// Test that if translation is a success, timeout is not triggered.
TEST_F(IOSTranslateDriverTest, TestNoTimeout) {
  driver_->PrepareToTranslatePage(0, "en", "fr", true);
  EXPECT_CALL(*mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, _, _,
                              TranslateErrors::NONE, _))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(*mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, _, _,
                              TranslateErrors::TRANSLATION_TIMEOUT, _))
      .Times(0);
  driver_->OnTranslateComplete(TranslateErrors::NONE, "en", 1.0);
  task_environment_.AdvanceClock(base::Seconds(10));
  task_environment_.RunUntilIdle();
}

}  // namespace translate
