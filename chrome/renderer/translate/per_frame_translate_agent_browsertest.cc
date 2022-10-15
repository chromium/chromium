// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/per_frame_translate_agent.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

class PerFrameTranslateAgent : public translate::PerFrameTranslateAgent {
 public:
  explicit PerFrameTranslateAgent(content::RenderFrame* render_frame)
      : translate::PerFrameTranslateAgent(
            render_frame,
            ISOLATED_WORLD_ID_TRANSLATE,
            render_frame->GetAssociatedInterfaceRegistry()) {}

  PerFrameTranslateAgent(const PerFrameTranslateAgent&) = delete;
  PerFrameTranslateAgent& operator=(const PerFrameTranslateAgent&) = delete;

  base::TimeDelta AdjustDelay(int delayInMs) override {
    // Just returns base::TimeDelta() which has initial value 0.
    // Tasks doesn't need to be delayed in tests.
    return base::TimeDelta();
  }

  void CallGetWebLanguageDetectionDetails() {
    // Reset result values firstly.
    detected_language_details_ = false;
    detected_content_meta_lang_ = absl::nullopt;
    detected_html_root_lang_ = absl::nullopt;
    detected_has_notranslate_meta_ = false;

    // Will get new result values via OnWebLanguageDetectionDetails.
    GetWebLanguageDetectionDetails(
        base::BindOnce(&PerFrameTranslateAgent::OnWebLanguageDetectionDetails,
                       base::Unretained(this)));
  }

  bool GetDetectedDetails(std::string* content_meta_lang,
                          std::string* html_root_lang,
                          bool* has_notranslate_meta) {
    if (!detected_language_details_)
      return false;
    if (content_meta_lang)
      *content_meta_lang = *detected_content_meta_lang_;
    if (html_root_lang)
      *html_root_lang = *detected_html_root_lang_;
    if (has_notranslate_meta)
      *has_notranslate_meta = detected_has_notranslate_meta_;
    return true;
  }

  void CallTranslateFrame(const std::string& source_lang,
                          const std::string& target_lang,
                          const std::string& translate_script) {
    // Reset result values firstly.
    page_translated_ = false;
    trans_result_cancelled_ = false;
    trans_result_source_lang_ = absl::nullopt;
    trans_result_translated_lang_ = absl::nullopt;
    trans_result_error_type_ = translate::TranslateErrors::NONE;

    // Will get new result values via OnPageTranslated.
    TranslateFrame(translate_script, source_lang, target_lang,
                   base::BindOnce(&PerFrameTranslateAgent::OnPageTranslated,
                                  base::Unretained(this)));
  }

  bool GetPageTranslatedResult(std::string* source_lang,
                               std::string* target_lang,
                               translate::TranslateErrors* error) {
    if (!page_translated_)
      return false;
    if (source_lang)
      *source_lang = *trans_result_source_lang_;
    if (target_lang)
      *target_lang = *trans_result_translated_lang_;
    if (error)
      *error = trans_result_error_type_;
    return true;
  }

  MOCK_METHOD0(IsTranslateLibAvailable, bool());
  MOCK_METHOD0(IsTranslateLibReady, bool());
  MOCK_METHOD0(HasTranslationFinished, bool());
  MOCK_METHOD0(HasTranslationFailed, bool());
  MOCK_METHOD0(GetPageSourceLanguage, std::string());
  MOCK_METHOD0(GetErrorCode, int64_t());
  MOCK_METHOD0(StartTranslation, bool());
  MOCK_METHOD1(ExecuteScript, void(const std::string&));
  MOCK_METHOD2(ExecuteScriptAndGetBoolResult, bool(const std::string&, bool));
  MOCK_METHOD1(ExecuteScriptAndGetStringResult,
               std::string(const std::string&));
  MOCK_METHOD1(ExecuteScriptAndGetDoubleResult, double(const std::string&));
  MOCK_METHOD1(ExecuteScriptAndGetIntegerResult, int64_t(const std::string&));

 private:
  void OnWebLanguageDetectionDetails(const std::string& content_meta_language,
                                     const std::string& html_root_lang,
                                     const GURL& url,
                                     bool has_notranslate_meta) {
    detected_language_details_ = true;
    detected_content_meta_lang_ = content_meta_language;
    detected_html_root_lang_ = html_root_lang;
    detected_has_notranslate_meta_ = has_notranslate_meta;
  }

  void OnPageTranslated(bool cancelled,
                        const std::string& source_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors error_type) {
    page_translated_ = true;
    trans_result_cancelled_ = cancelled;
    trans_result_source_lang_ = source_lang;
    trans_result_translated_lang_ = translated_lang;
    trans_result_error_type_ = error_type;
  }

  bool detected_language_details_;
  absl::optional<std::string> detected_content_meta_lang_;
  absl::optional<std::string> detected_html_root_lang_;
  bool detected_has_notranslate_meta_;

  bool page_translated_;
  bool trans_result_cancelled_;
  absl::optional<std::string> trans_result_source_lang_;
  absl::optional<std::string> trans_result_translated_lang_;
  translate::TranslateErrors trans_result_error_type_;
};

class PerFrameTranslateAgentBrowserTest : public ChromeRenderViewTest {
 public:
  PerFrameTranslateAgentBrowserTest() : translate_agent_(nullptr) {}

  PerFrameTranslateAgentBrowserTest(const PerFrameTranslateAgentBrowserTest&) =
      delete;
  PerFrameTranslateAgentBrowserTest& operator=(
      const PerFrameTranslateAgentBrowserTest&) = delete;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    translate_agent_ = new PerFrameTranslateAgent(GetMainRenderFrame());
  }

  void TearDown() override {
    delete translate_agent_;
    ChromeRenderViewTest::TearDown();
  }

  PerFrameTranslateAgent* translate_agent_;
};

// Tests that the browser gets notified of the translation failure if the
// translate library fails/times-out during initialization.
TEST_F(PerFrameTranslateAgentBrowserTest, TranslateLibNeverReady) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_agent_, IsTranslateLibReady())
      .Times(AtLeast(5))  // See kMaxTranslateInitCheckAttempts in
                          // translate_agent.cc
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*translate_agent_, GetErrorCode())
      .Times(AtLeast(5))
      .WillRepeatedly(
          Return(base::to_underlying(translate::TranslateErrors::NONE)));

  translate_agent_->CallTranslateFrame("en", "fr", std::string());
  base::RunLoop().RunUntilIdle();

  translate::TranslateErrors error;
  ASSERT_TRUE(
      translate_agent_->GetPageTranslatedResult(nullptr, nullptr, &error));
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_TIMEOUT, error);
}

// Tests that the browser gets notified of the translation success when the
// translation succeeds.
TEST_F(PerFrameTranslateAgentBrowserTest, TranslateSuccess) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_agent_, IsTranslateLibReady())
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_agent_, GetErrorCode())
      .WillOnce(Return(base::to_underlying(translate::TranslateErrors::NONE)));

  EXPECT_CALL(*translate_agent_, StartTranslation()).WillOnce(Return(true));

  // Succeed after few checks.
  EXPECT_CALL(*translate_agent_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_agent_, HasTranslationFinished())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_agent_, ExecuteScriptAndGetDoubleResult(_)).Times(3);

  std::string source_lang("en");
  std::string target_lang("fr");
  translate_agent_->CallTranslateFrame(source_lang, target_lang, std::string());
  base::RunLoop().RunUntilIdle();

  std::string received_source_lang;
  std::string received_target_lang;
  translate::TranslateErrors error;
  ASSERT_TRUE(translate_agent_->GetPageTranslatedResult(
      &received_source_lang, &received_target_lang, &error));
  EXPECT_EQ(source_lang, received_source_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
}

// Tests that the browser gets notified of the translation failure when the
// translation fails.
TEST_F(PerFrameTranslateAgentBrowserTest, TranslateFailure) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_agent_, IsTranslateLibReady()).WillOnce(Return(true));

  EXPECT_CALL(*translate_agent_, StartTranslation()).WillOnce(Return(true));

  // Fail after few checks.
  EXPECT_CALL(*translate_agent_, HasTranslationFailed())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_agent_, HasTranslationFinished())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*translate_agent_, GetErrorCode())
      .WillOnce(Return(
          base::to_underlying(translate::TranslateErrors::TRANSLATION_ERROR)));

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_agent_, ExecuteScriptAndGetDoubleResult(_)).Times(2);

  translate_agent_->CallTranslateFrame("en", "fr", std::string());
  base::RunLoop().RunUntilIdle();

  translate::TranslateErrors error;
  ASSERT_TRUE(
      translate_agent_->GetPageTranslatedResult(nullptr, nullptr, &error));
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR, error);
}

// Tests that when the browser translate a page for which the language is
// undefined we query the translate element to get the language.
TEST_F(PerFrameTranslateAgentBrowserTest, UndefinedSourceLang) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_agent_, IsTranslateLibReady()).WillOnce(Return(true));

  EXPECT_CALL(*translate_agent_, GetPageSourceLanguage())
      .WillOnce(Return("de"));

  EXPECT_CALL(*translate_agent_, StartTranslation()).WillOnce(Return(true));
  EXPECT_CALL(*translate_agent_, HasTranslationFailed())
      .WillOnce(Return(false));
  EXPECT_CALL(*translate_agent_, HasTranslationFinished())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_agent_, ExecuteScriptAndGetDoubleResult(_)).Times(3);

  translate_agent_->CallTranslateFrame(translate::kUnknownLanguageCode, "fr",
                                       std::string());
  base::RunLoop().RunUntilIdle();

  translate::TranslateErrors error;
  std::string source_lang;
  std::string target_lang;
  ASSERT_TRUE(translate_agent_->GetPageTranslatedResult(&source_lang,
                                                        &target_lang, &error));
  EXPECT_EQ("de", source_lang);
  EXPECT_EQ("fr", target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
}

// Tests that starting a translation while a similar one is pending does not
// break anything.
TEST_F(PerFrameTranslateAgentBrowserTest, MultipleSimilarTranslations) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_agent_, IsTranslateLibReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_agent_, StartTranslation())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_agent_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_agent_, HasTranslationFinished())
      .WillOnce(Return(true));

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_agent_, ExecuteScriptAndGetDoubleResult(_)).Times(3);

  std::string source_lang("en");
  std::string target_lang("fr");
  translate_agent_->CallTranslateFrame(source_lang, target_lang, std::string());
  // While this is running call again CallTranslateFrame to make sure noting bad
  // happens.
  translate_agent_->CallTranslateFrame(source_lang, target_lang, std::string());
  base::RunLoop().RunUntilIdle();

  std::string received_source_lang;
  std::string received_target_lang;
  translate::TranslateErrors error;
  ASSERT_TRUE(translate_agent_->GetPageTranslatedResult(
      &received_source_lang, &received_target_lang, &error));
  EXPECT_EQ(source_lang, received_source_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
}

// Tests that starting a translation while a different one is pending works.
TEST_F(PerFrameTranslateAgentBrowserTest, MultipleDifferentTranslations) {
  EXPECT_CALL(*translate_agent_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_agent_, IsTranslateLibReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_agent_, StartTranslation())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_agent_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_agent_, HasTranslationFinished())
      .WillOnce(Return(true));

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_agent_, ExecuteScriptAndGetDoubleResult(_)).Times(5);

  std::string source_lang("en");
  std::string target_lang("fr");
  translate_agent_->CallTranslateFrame(source_lang, target_lang, std::string());
  // While this is running call again CallTranslateFrame with a new target lang.
  std::string new_target_lang("de");
  translate_agent_->CallTranslateFrame(source_lang, new_target_lang,
                                       std::string());
  base::RunLoop().RunUntilIdle();

  std::string received_source_lang;
  std::string received_target_lang;
  translate::TranslateErrors error;
  ASSERT_TRUE(translate_agent_->GetPageTranslatedResult(
      &received_source_lang, &received_target_lang, &error));
  EXPECT_EQ(source_lang, received_source_lang);
  EXPECT_EQ(new_target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
}

// Tests web language detection of the "notranslate" meta-tag.
TEST_F(PerFrameTranslateAgentBrowserTest,
       GetWebLanguageDetectionDetails_NoTranslateMetadata) {
  LoadHTML("<html><body>A random page with random content.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  std::string detected_content_meta_lang;
  std::string detected_html_root_lang;
  bool has_notranslate_meta;
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  ASSERT_FALSE(has_notranslate_meta);

  // Now the page specifies the META tag to prevent translation.
  LoadHTML(
      "<html lang=\"en\"><head><meta name=\"google\" "
      "value=\"notranslate\"></head>"
      "<body>A random page with random content.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  ASSERT_TRUE(has_notranslate_meta);
  EXPECT_EQ("en", detected_html_root_lang);
  EXPECT_EQ("", detected_content_meta_lang);

  // Try the alternate version of the META tag (content instead of value).
  LoadHTML(
      "<html lang=\"en\"><head><meta name=\"google\" "
      "content=\"notranslate\"></head>"
      "<body>A random page with random content.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  ASSERT_TRUE(has_notranslate_meta);
  EXPECT_EQ("en", detected_html_root_lang);
  EXPECT_EQ("", detected_content_meta_lang);
}

// Tests web language detection of content-language meta tag.
TEST_F(PerFrameTranslateAgentBrowserTest,
       GetWebLanguageDetectionDetails_LanguageMetaTag) {
  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"es\">"
      "</head><body>A random page with random content.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  std::string detected_content_meta_lang;
  std::string detected_html_root_lang;
  bool has_notranslate_meta;
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  ASSERT_FALSE(has_notranslate_meta);
  EXPECT_EQ("es", detected_content_meta_lang);
  EXPECT_EQ("", detected_html_root_lang);

  // Makes sure we support multiple languages specified.
  LoadHTML(
      "<html lang=\"en,fr\"><head><meta http-equiv=\"content-language\" "
      "content=\" fr , es,en \">"
      "</head><body>A random page with random content.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  EXPECT_EQ(" fr , es,en ", detected_content_meta_lang);
  EXPECT_EQ("en,fr", detected_html_root_lang);
}

// Tests web language detection for a back navigation.
TEST_F(PerFrameTranslateAgentBrowserTest,
       GetWebLanguageDetectionDetails_NavBack) {
  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"es\">"
      "</head><body>This page is in Spanish.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  std::string detected_content_meta_lang;
  std::string detected_html_root_lang;
  bool has_notranslate_meta;
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  EXPECT_EQ("es", detected_content_meta_lang);

  blink::PageState back_state = GetCurrentPageState();

  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"fr\">"
      "</head><body>This page is in French.</body></html>");
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  EXPECT_EQ("fr", detected_content_meta_lang);

  GoBack(GURL("data:text/html;charset=utf-8,<html><head>"
              "<meta http-equiv=\"content-language\" content=\"es\">"
              "</head><body>This page is in Spanish.</body></html>"),
         back_state);
  translate_agent_->CallGetWebLanguageDetectionDetails();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(translate_agent_->GetDetectedDetails(&detected_content_meta_lang,
                                                   &detected_html_root_lang,
                                                   &has_notranslate_meta));
  EXPECT_EQ("es", detected_content_meta_lang);
}
