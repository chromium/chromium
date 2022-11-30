// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/per_frame_content_translate_driver.h"

#include "base/strings/utf_string_conversions.h"
#include "components/services/language_detection/public/mojom/language_detection.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

class DriverObserver
    : public ContentTranslateDriver::LanguageDetectionObserver {
 public:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override {
    details_ = details;
  }

  const translate::LanguageDetectionDetails& GetObservedDetails() const {
    return details_;
  }

 private:
  translate::LanguageDetectionDetails details_;
};

class PerFrameContentTranslateDriverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    driver_ = std::make_unique<PerFrameContentTranslateDriver>(
        *web_contents(), nullptr /* url_language_histogram */);
    driver_->AddLanguageDetectionObserver(&observer_);
  }

  void TearDown() override {
    driver_->RemoveLanguageDetectionObserver(&observer_);
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void OnWebLanguageDetectionDetails(
      mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent,
      const std::string& content_language,
      const std::string& html_lang,
      const GURL& url,
      bool has_no_translate_meta) {
    driver_->OnWebLanguageDetectionDetails(std::move(translate_agent),
                                           content_language, html_lang, url,
                                           has_no_translate_meta);
  }

  void OnPageContentsLanguage(const std::string& contents_language,
                              bool is_contents_language_reliable) {
    mojo::Remote<language_detection::mojom::LanguageDetectionService>
        service_handle;
    driver_->OnPageContentsLanguage(std::move(service_handle),
                                    contents_language,
                                    is_contents_language_reliable);
  }

  const std::string& GetAdoptedLanguage() const {
    return observer_.GetObservedDetails().adopted_language;
  }

  bool HasGoodContentDetection() const {
    return observer_.GetObservedDetails().is_model_reliable;
  }

  bool DoNotTranslate() const {
    return observer_.GetObservedDetails().has_notranslate;
  }

 private:
  std::unique_ptr<PerFrameContentTranslateDriver> driver_;
  DriverObserver observer_;
};

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_MetaTagOverridesUnd) {
  OnPageContentsLanguage("und", false /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(std::move(translate_agent), "en" /* meta */,
                                "" /* html */, GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_FALSE(DoNotTranslate());
  EXPECT_FALSE(HasGoodContentDetection());
  EXPECT_EQ("en", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_HtmlLangOverridesMetaTag) {
  OnPageContentsLanguage("und", false /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(std::move(translate_agent), "en" /* meta */,
                                "fr" /* html */, GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("fr", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_ContentOverridesMetaTag) {
  OnPageContentsLanguage("es", true /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(std::move(translate_agent), "en" /* meta */,
                                "" /* html */, GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_TRUE(HasGoodContentDetection());
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_ContentOverridesHtmlLang) {
  OnPageContentsLanguage("es", true /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(
      std::move(translate_agent), "en" /* meta */, "es-MX" /* html */,
      GURL("https://whatever.com"), false /* notranslate */);
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_NoTranslateMetaTag) {
  OnPageContentsLanguage("es", true /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(std::move(translate_agent), "en" /* meta */,
                                "" /* html */, GURL("https://whatever.com"),
                                true /* notranslate */);
  EXPECT_TRUE(DoNotTranslate());
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_LanguageFormatVariants) {
  OnPageContentsLanguage("und", false /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent;
  OnWebLanguageDetectionDetails(std::move(translate_agent), "ZH_tw" /* meta */,
                                "" /* html */, GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("zh-TW", GetAdoptedLanguage());

  OnPageContentsLanguage("und", false /* is_reliable */);
  mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent2;
  OnWebLanguageDetectionDetails(
      std::move(translate_agent2), " fr , es,en " /* meta */, "" /* html */,
      GURL("https://whatever.com"), false /* notranslate */);
  EXPECT_EQ("fr", GetAdoptedLanguage());
}

}  // namespace translate
