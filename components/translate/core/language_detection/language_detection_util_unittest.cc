// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_util.h"

#include <string>

#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/language_detection/core/constants.h"
#include "components/translate/core/common/translate_language_matcher.h"
#include "components/translate/core/common/translate_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

// Text long enough for a reliable detection, in Chinese (Traditional) and in
// Chinese (Simplified).
constexpr char16_t kTraditionalChineseContents[] =
    u"網頁瀏覽器是一種用於檢索並展示全球資訊網資訊資源的應用程式。這些資訊資源"
    u"可為網頁、圖片、影音或其他內容，它們由統一資源標誌符標誌。資訊資源中的超"
    u"連結可使使用者方便地瀏覽相關資訊。";
constexpr char16_t kSimplifiedChineseContents[] =
    u"网页浏览器是一种用于检索并展示万维网信息资源的应用程序。这些信息资源可为"
    u"网页、图片、影音或其他内容，它们由统一资源标志符标志。信息资源中的超链接"
    u"可使用户方便地浏览相关信息。";

// Tests that well-known language code typos are fixed.
TEST(LanguageDetectionUtilTest, LanguageCodeTypoCorrection) {
  std::string language;

  // Strip the second and later codes.
  language = std::string("ja,en");
  translate::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja", language);

  // Replace dash with hyphen.
  language = std::string("ja_JP");
  translate::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);

  // Correct wrong cases.
  language = std::string("JA-jp");
  translate::CorrectLanguageCodeTypo(&language);
  EXPECT_EQ("ja-JP", language);
}

// Tests if the language codes' format is invalid.
TEST(LanguageDetectionUtilTest, IsValidLanguageCode) {
  std::string language;

  language = std::string("ja");
  EXPECT_TRUE(translate::IsValidLanguageCode(language));

  language = std::string("ja-JP");
  EXPECT_TRUE(translate::IsValidLanguageCode(language));

  language = std::string("ceb");
  EXPECT_TRUE(translate::IsValidLanguageCode(language));

  language = std::string("ceb-XX");
  EXPECT_TRUE(translate::IsValidLanguageCode(language));

  // Invalid because the sub code consists of a number.
  language = std::string("utf-8");
  EXPECT_FALSE(translate::IsValidLanguageCode(language));

  // Invalid because of six characters after hyphen.
  language = std::string("ja-YUKARI");
  EXPECT_FALSE(translate::IsValidLanguageCode(language));

  // Invalid because of four characters.
  language = std::string("DHMO");
  EXPECT_FALSE(translate::IsValidLanguageCode(language));
}

// Tests that well-known languages which often have wrong server configuration
// are handles.
TEST(LanguageDetectionUtilTest, WellKnownWrongConfiguration) {
  EXPECT_TRUE(translate::MaybeServerWrongConfiguration("en", "ja"));
  EXPECT_TRUE(translate::MaybeServerWrongConfiguration("en-US", "ja"));
  EXPECT_TRUE(translate::MaybeServerWrongConfiguration("en", "zh-CN"));
  EXPECT_FALSE(translate::MaybeServerWrongConfiguration("ja", "en"));
  EXPECT_FALSE(translate::MaybeServerWrongConfiguration("en", "he"));
}

// Tests that the language meta tag providing wrong information is ignored by
// LanguageDetectionUtil due to disagreement between meta tag and CLD.
TEST(LanguageDetectionUtilTest, CLDDisagreeWithWrongLanguageCode) {
  base::HistogramTester histogram_tester;
  std::u16string contents =
      u"<html><head><meta http-equiv='Content-Language' content='ja'></head>"
      u"<body>This is a page apparently written in English. Even though "
      u"content-language is provided, the value will be ignored if the value "
      u"is suspicious.</body></html>";
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::string language = translate::DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  EXPECT_EQ(language_detection::kUnknownLanguageCode, language);
  EXPECT_EQ("en", model_detected_language);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_GT(model_reliability_score, 0.5);
}

// Tests that the language meta tag providing "en-US" style information is
// agreed by CLD.
TEST(LanguageDetectionUtilTest, CLDAgreeWithLanguageCodeHavingCountryCode) {
  base::HistogramTester histogram_tester;
  std::u16string contents =
      u"<html><head><meta http-equiv='Content-Language' content='en-US'></head>"
      u"<body>This is a page apparently written in English. Even though "
      u"content-language is provided, the value will be ignored if the value "
      u"is suspicious.</body></html>";
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::string language = translate::DeterminePageLanguage(
      std::string("en-US"), std::string(), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  EXPECT_EQ("en", language);
  EXPECT_EQ("en", model_detected_language);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_GT(model_reliability_score, 0.5);
}

// Tests that the language meta tag providing wrong information is ignored and
// CLD's language will be adopted by LanguageDetectionUtil due to an invalid
// meta tag.
TEST(LanguageDetectionUtilTest, InvalidLanguageMetaTagProviding) {
  base::HistogramTester histogram_tester;
  std::u16string contents =
      u"<html><head><meta http-equiv='Content-Language' content='utf-8'></head>"
      u"<body>This is a page apparently written in English. Even though "
      u"content-language is provided, the value will be ignored and CLD's"
      u" language will be adopted if the value is invalid.</body></html>";
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::string language = translate::DeterminePageLanguage(
      std::string("utf-8"), std::string(), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  EXPECT_EQ("en", language);
  EXPECT_EQ("en", model_detected_language);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_GT(model_reliability_score, 0.5);
}

// Tests that the language meta tag providing wrong information is ignored
// because of valid html lang attribute.
TEST(LanguageDetectionUtilTest, AdoptHtmlLang) {
  base::HistogramTester histogram_tester;
  std::u16string contents =
      u"<html lang='en'><head><meta http-equiv='Content-Language' content='ja'>"
      u"</head><body>This is a page apparently written in English. Even though "
      u"content-language is provided, the value will be ignored if the value "
      u"is suspicious.</body></html>";
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::string language = translate::DeterminePageLanguage(
      std::string("ja"), std::string("en"), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  EXPECT_EQ("en", language);
  EXPECT_EQ("en", model_detected_language);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_GT(model_reliability_score, 0.5);
}

// Tests that languages that often have the wrong server configuration are
// correctly identified. All incorrect language codes should be checked to
// make sure the binary_search is correct.
TEST(LanguageDetectionUtilTest, IsServerWrongConfigurationLanguage) {
  // These languages should all be identified as having the wrong server
  // configuration.
  const char* const wrong_languages[] = {"es", "pt",    "ja",    "ru",
                                         "de", "zh-CN", "zh-TW", "ar",
                                         "id", "fr",    "it",    "th"};
  for (const char* const language : wrong_languages) {
    EXPECT_TRUE(translate::IsServerWrongConfigurationLanguage(language));
  }
  // These languages should all be identified as having the right server
  // configuration.
  const char* const right_languages[] = {"en", "en-AU", "en-US",
                                         "xx", "gg",    "rr"};
  for (const char* const language : right_languages) {
    EXPECT_FALSE(translate::IsServerWrongConfigurationLanguage(language));
  }
}

TEST(LanguageDetectionUtilTest, ShortContentHttpLanguage) {
  std::string model_detected_language;
  bool is_model_reliable = false;
  float model_reliability_score = 0.0;
  std::string language = translate::DeterminePageLanguage(
      "fr", "", u"123456", &model_detected_language, &is_model_reliable,
      model_reliability_score);
  EXPECT_EQ("fr", language);
  EXPECT_EQ("und", model_detected_language);
}

// Tests that the model determines whether a page declaring a generic Chinese
// language ("zh") is in Chinese (Simplified) or Chinese (Traditional).
TEST(LanguageDetectionUtilTest, ModelComplementsGenericChinese) {
  base::HistogramTester histogram_tester;

  // From the html lang attribute.
  EXPECT_EQ("zh-TW", translate::DeterminePageLanguage(
                         /*code=*/"", /*html_lang=*/"zh",
                         /*model_detected_language=*/"zh-TW",
                         /*is_model_reliable=*/true));
  EXPECT_EQ("zh-CN", translate::DeterminePageLanguage(
                         /*code=*/"", /*html_lang=*/"zh",
                         /*model_detected_language=*/"zh-CN",
                         /*is_model_reliable=*/true));

  // From the Content-Language.
  EXPECT_EQ("zh-TW", translate::DeterminePageLanguage(
                         /*code=*/"zh", /*html_lang=*/"",
                         /*model_detected_language=*/"zh-TW",
                         /*is_model_reliable=*/true));
  EXPECT_EQ("zh-CN", translate::DeterminePageLanguage(
                         /*code=*/"zh", /*html_lang=*/"",
                         /*model_detected_language=*/"zh-CN",
                         /*is_model_reliable=*/true));

  histogram_tester.ExpectUniqueSample(
      "Translate.LanguageDetection.LanguageVerification",
      translate::LanguageVerificationType::kModelComplementsCountry, 4);
}

// Tests that a generic Chinese language ("zh") declared by the page is kept
// when there is no model language, instead of being resolved to a script.
TEST(LanguageDetectionUtilTest, GenericChineseWithoutModelLanguage) {
  EXPECT_EQ("zh", translate::DeterminePageLanguage(
                      /*code=*/"", /*html_lang=*/"zh",
                      language_detection::kUnknownLanguageCode,
                      /*is_model_reliable=*/false));
  EXPECT_EQ("zh", translate::DeterminePageLanguage(
                      /*code=*/"zh", /*html_lang=*/"",
                      language_detection::kUnknownLanguageCode,
                      /*is_model_reliable=*/false));
  EXPECT_EQ("zh", translate::DeterminePageLanguageNoModel(
                      /*content_lang=*/"", /*html_lang=*/"zh",
                      translate::LanguageVerificationType::kNoPageContent));
  EXPECT_EQ("zh", translate::DeterminePageLanguageNoModel(
                      /*content_lang=*/"zh", /*html_lang=*/"",
                      translate::LanguageVerificationType::kNoPageContent));
}

// Tests that Chinese with a script or a region declared by the page is matched
// to a Translate language, and that it is preferred over the model language.
TEST(LanguageDetectionUtilTest, ChineseWithScriptOrRegion) {
  struct {
    const char* html_lang;
    const char* expected_language;
  } kTestCases[] = {
      {"zh-TW", "zh-TW"}, {"zh-HK", "zh-TW"}, {"zh-Hant", "zh-TW"},
      {"zh-CN", "zh-CN"}, {"zh-SG", "zh-CN"}, {"zh-Hans", "zh-CN"},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.html_lang);
    EXPECT_EQ(test_case.expected_language,
              translate::DeterminePageLanguage(
                  /*code=*/"", test_case.html_lang,
                  language_detection::kUnknownLanguageCode,
                  /*is_model_reliable=*/false));
  }

  EXPECT_EQ("zh-CN", translate::DeterminePageLanguage(
                         /*code=*/"", /*html_lang=*/"zh-CN",
                         /*model_detected_language=*/"zh-TW",
                         /*is_model_reliable=*/true));
  EXPECT_EQ("zh-TW", translate::DeterminePageLanguage(
                         /*code=*/"", /*html_lang=*/"zh-TW",
                         /*model_detected_language=*/"zh-CN",
                         /*is_model_reliable=*/true));
}

// Tests that the page contents determine whether a page declaring a generic
// Chinese language ("zh") is in Chinese (Simplified) or Chinese (Traditional),
// and that "zh" is kept when the contents are too short for a reliable
// detection.
TEST(LanguageDetectionUtilTest, GenericChineseHtmlLangWithContents) {
  std::string model_detected_language;
  bool is_model_reliable = false;
  float model_reliability_score = 0.0;

  std::string language = translate::DeterminePageLanguage(
      /*code=*/"", /*html_lang=*/"zh", kTraditionalChineseContents,
      &model_detected_language, &is_model_reliable, model_reliability_score);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_EQ("zh-TW", model_detected_language);
  EXPECT_EQ("zh-TW", language);

  language = translate::DeterminePageLanguage(
      /*code=*/"", /*html_lang=*/"zh", kSimplifiedChineseContents,
      &model_detected_language, &is_model_reliable, model_reliability_score);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_EQ("zh-CN", model_detected_language);
  EXPECT_EQ("zh-CN", language);

  language = translate::DeterminePageLanguage(
      /*code=*/"", /*html_lang=*/"zh", u"歡迎", &model_detected_language,
      &is_model_reliable, model_reliability_score);
  EXPECT_FALSE(is_model_reliable);
  EXPECT_EQ("und", model_detected_language);
  EXPECT_EQ("zh", language);
}

}  // namespace
}  // namespace translate
