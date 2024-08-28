// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_agent.h"

#include <tuple>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

std::string UpdateGURLScheme(GURL url, const char scheme[]) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  return url.ReplaceComponents(replacements).spec();
}

class FakeContentTranslateDriver
    : public translate::mojom::ContentTranslateDriver {
 public:
  FakeContentTranslateDriver() = default;
  ~FakeContentTranslateDriver() override = default;

  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this, mojo::PendingReceiver<translate::mojom::ContentTranslateDriver>(
                  std::move(handle)));
  }

  // translate::mojom::ContentTranslateDriver implementation.
  void RegisterPage(
      mojo::PendingRemote<translate::mojom::TranslateAgent> translate_agent,
      const translate::LanguageDetectionDetails& details,
      bool page_level_translation_criteria_met) override {
    called_new_page_ = true;
    details_ = details;
    page_level_translation_criteria_met_ = page_level_translation_criteria_met;
  }

  void ResetNewPageValues() {
    called_new_page_ = false;
    details_ = std::nullopt;
    page_level_translation_criteria_met_ = false;
  }

  bool called_new_page_ = false;
  bool page_level_translation_criteria_met_ = false;
  std::optional<translate::LanguageDetectionDetails> details_;

 private:
  mojo::ReceiverSet<translate::mojom::ContentTranslateDriver> receivers_;
};

// Load the model file at the provided file path.
base::File LoadModelFile(const base::FilePath& model_file_path) {
  if (!base::PathExists(model_file_path))
    return base::File();

  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("translate")
      .AppendASCII("valid_model.tflite");
}

}  // namespace

class TestTranslateAgent : public translate::TranslateAgent {
 public:
  explicit TestTranslateAgent(content::RenderFrame* render_frame)
      : translate::TranslateAgent(render_frame, ISOLATED_WORLD_ID_TRANSLATE) {}

  TestTranslateAgent(const TestTranslateAgent&) = delete;
  TestTranslateAgent& operator=(const TestTranslateAgent&) = delete;

  base::TimeDelta AdjustDelay(int delayInMs) override {
    // Just returns base::TimeDelta() which has initial value 0.
    // Tasks doesn't need to be delayed in tests.
    return base::TimeDelta();
  }

  void TranslatePage(const std::string& source_lang,
                     const std::string& target_lang,
                     const std::string& translate_script) {
    // Reset result values firstly.
    page_translated_ = false;
    trans_result_cancelled_ = false;
    trans_result_source_lang_ = std::nullopt;
    trans_result_translated_lang_ = std::nullopt;
    trans_result_error_type_ = translate::TranslateErrors::NONE;

    // Will get new result values via OnPageTranslated.
    TranslateFrame(translate_script, source_lang, target_lang,
                   base::BindOnce(&TestTranslateAgent::OnPageTranslated,
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

  bool page_translated_;
  bool trans_result_cancelled_;
  std::optional<std::string> trans_result_source_lang_;
  std::optional<std::string> trans_result_translated_lang_;
  translate::TranslateErrors trans_result_error_type_;
};

class TranslateAgentBrowserTest : public ChromeRenderViewTest {
 public:
  TranslateAgentBrowserTest() : translate_agent_(nullptr) {}

  TranslateAgentBrowserTest(const TranslateAgentBrowserTest&) = delete;
  TranslateAgentBrowserTest& operator=(const TranslateAgentBrowserTest&) =
      delete;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        translate::kTFLiteLanguageDetectionEnabled);
    translate_agent_ = new TestTranslateAgent(GetMainRenderFrame());

    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        translate::mojom::ContentTranslateDriver::Name_,
        base::BindRepeating(&FakeContentTranslateDriver::BindHandle,
                            base::Unretained(&fake_translate_driver_)));
    base::File model_file = LoadModelFile(model_file_path());
    translate_agent_->SeedLanguageDetectionModelForTesting(
        std::move(model_file));
  }

  void TearDown() override {
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        translate::mojom::ContentTranslateDriver::Name_, {});

    delete translate_agent_;
    ChromeRenderViewTest::TearDown();
  }

  raw_ptr<TestTranslateAgent, DanglingUntriaged> translate_agent_;
  FakeContentTranslateDriver fake_translate_driver_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the browser gets notified of the translation failure if the
// translate library fails/times-out during initialization.
TEST_F(TranslateAgentBrowserTest, TranslateLibNeverReady) {
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

  translate_agent_->TranslatePage("en", "fr", std::string());
  base::RunLoop().RunUntilIdle();

  translate::TranslateErrors error;
  ASSERT_TRUE(
      translate_agent_->GetPageTranslatedResult(nullptr, nullptr, &error));
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_TIMEOUT, error);
}

// Tests that the browser gets notified of the translation success when the
// translation succeeds.
TEST_F(TranslateAgentBrowserTest, TranslateSuccess) {
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
  translate_agent_->TranslatePage(source_lang, target_lang, std::string());
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
TEST_F(TranslateAgentBrowserTest, TranslateFailure) {
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

  translate_agent_->TranslatePage("en", "fr", std::string());
  base::RunLoop().RunUntilIdle();

  translate::TranslateErrors error;
  ASSERT_TRUE(
      translate_agent_->GetPageTranslatedResult(nullptr, nullptr, &error));
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR, error);
}

// Tests that when the browser translate a page for which the language is
// undefined we query the translate element to get the language.
TEST_F(TranslateAgentBrowserTest, UndefinedSourceLang) {
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

  translate_agent_->TranslatePage(translate::kUnknownLanguageCode, "fr",
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
TEST_F(TranslateAgentBrowserTest, MultipleSimilarTranslations) {
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
  translate_agent_->TranslatePage(source_lang, target_lang, std::string());
  // While this is running call again TranslatePage to make sure noting bad
  // happens.
  translate_agent_->TranslatePage(source_lang, target_lang, std::string());
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
TEST_F(TranslateAgentBrowserTest, MultipleDifferentTranslations) {
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
  translate_agent_->TranslatePage(source_lang, target_lang, std::string());
  // While this is running call again TranslatePage with a new target lang.
  std::string new_target_lang("de");
  translate_agent_->TranslatePage(source_lang, new_target_lang, std::string());
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

// Tests that we send the right translate language message for a page and that
// we respect the "no translate" meta-tag.
TEST_F(TranslateAgentBrowserTest, TranslatablePage) {
  LoadHTML("<html><body>A random page with random content.</body></html>");

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_TRUE(fake_translate_driver_.page_level_translation_criteria_met_)
      << "Page should be translatable.";
  fake_translate_driver_.ResetNewPageValues();

  // Now the page specifies the META tag to prevent translation.
  LoadHTML(
      "<html><head><meta name=\"google\" value=\"notranslate\"></head>"
      "<body>A random page with random content.</body></html>");

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_)
      << "Page should not be translatable.";
  fake_translate_driver_.ResetNewPageValues();

  // Try the alternate version of the META tag (content instead of value).
  LoadHTML(
      "<html><head><meta name=\"google\" content=\"notranslate\"></head>"
      "<body>A random page with random content.</body></html>");

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_)
      << "Page should not be translatable.";
}

// Tests that the language meta tag takes precedence over the CLD when reporting
// the page's language.
TEST_F(TranslateAgentBrowserTest, LanguageMetaTag) {
  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"es\">"
      "</head><body>A</body></html>");
  //   "</head><body>Esta página está en español.</body></html>");

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("es", fake_translate_driver_.details_->adopted_language);
  fake_translate_driver_.ResetNewPageValues();

  // Makes sure we support multiple languages specified.
  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" "
      "content=\" fr , es,en \">"
      "</head><body>Cette page est en français.</body></html>");

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("fr", fake_translate_driver_.details_->adopted_language);
}

// Tests that the language meta tag works even with non-all-lower-case.
// http://code.google.com/p/chromium/issues/detail?id=145689
TEST_F(TranslateAgentBrowserTest, LanguageMetaTagCase) {
  LoadHTML(
      "<html><head><meta http-equiv=\"Content-Language\" content=\"es\">"
      "</head><body>E</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("es", fake_translate_driver_.details_->adopted_language);
  fake_translate_driver_.ResetNewPageValues();

  // Makes sure we support multiple languages specified.
  LoadHTML(
      "<html><head><meta http-equiv=\"Content-Language\" "
      "content=\" fr , es,en \">"
      "</head><body>Cette page est en français.</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("fr", fake_translate_driver_.details_->adopted_language);
}

// Tests that the language meta tag is converted to Chrome standard of dashes
// instead of underscores and proper capitalization.
// http://code.google.com/p/chromium/issues/detail?id=159487
TEST_F(TranslateAgentBrowserTest, LanguageCommonMistakesAreCorrected) {
  LoadHTML(
      "<html><head><meta http-equiv='Content-Language' content='EN_us'>"
      "</head><body>A random page with random content.</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("en", fake_translate_driver_.details_->adopted_language);
  fake_translate_driver_.ResetNewPageValues();

  LoadHTML(
      "<html><head><meta http-equiv='Content-Language' content='ZH_tw'>"
      "</head><body>A</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("zh-TW", fake_translate_driver_.details_->adopted_language);
}

// Tests that a back navigation gets a translate language message.
TEST_F(TranslateAgentBrowserTest, BackToTranslatablePage) {
  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"es\">"
      "</head><body>E</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("es", fake_translate_driver_.details_->adopted_language);
  fake_translate_driver_.ResetNewPageValues();

  blink::PageState back_state = GetCurrentPageState();

  LoadHTML(
      "<html><head><meta http-equiv=\"content-language\" content=\"fr\">"
      "</head><body>E</body></html>");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("fr", fake_translate_driver_.details_->adopted_language);
  fake_translate_driver_.ResetNewPageValues();

  GoBack(GURL("data:text/html;charset=utf-8,<html><head>"
              "<meta http-equiv=\"content-language\" content=\"es\">"
              "</head><body>E</body></html>"),
         back_state);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_translate_driver_.called_new_page_);
  EXPECT_EQ("es", fake_translate_driver_.details_->adopted_language);
}

TEST_F(TranslateAgentBrowserTest, UnsupportedTranslateSchemes) {
  GURL url("https://foo.com");
  LoadHTMLWithUrlOverride(
      "<html><body>A random page with random content.</body></html>",
      UpdateGURLScheme(url, content::kChromeUIScheme).c_str());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);

  LoadHTMLWithUrlOverride(
      "<html><body>A random page with random content.</body></html>",
      url::kAboutBlankURL);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);

  LoadHTMLWithUrlOverride(
      "<html><body>A random page with random content.</body></html>",
      UpdateGURLScheme(url, content::kChromeDevToolsScheme).c_str());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_translate_driver_.called_new_page_);
  EXPECT_FALSE(fake_translate_driver_.page_level_translation_criteria_met_);
}
