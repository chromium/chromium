// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

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

// Tests that similar language table works.
TEST(LanguageDetectionUtilTest, SimilarLanguageCode) {
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("en", "en"));
  EXPECT_FALSE(translate::IsSameOrSimilarLanguages("en", "ja"));

  // Language codes are same if the main parts are same. The synonyms should be
  // took into account (ex: 'iw' and 'he').
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("sr-ME", "sr"));
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("sr", "sr-ME"));
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("he", "he-IL"));
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("eng", "eng-US"));
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("eng-US", "eng"));
  EXPECT_FALSE(translate::IsSameOrSimilarLanguages("eng", "enm"));

  // Even though the main parts are different, some special language pairs are
  // recognized as same languages.
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("bs", "hr"));
  EXPECT_TRUE(translate::IsSameOrSimilarLanguages("ne", "hi"));
  EXPECT_FALSE(translate::IsSameOrSimilarLanguages("bs", "hi"));
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
  EXPECT_EQ(translate::kUnknownLanguageCode, language);
  EXPECT_EQ("en", model_detected_language);
  EXPECT_TRUE(is_model_reliable);
  EXPECT_GT(model_reliability_score, 0.5);
  histogram_tester.ExpectTotalCount(
      "Translate.CLD3.TopLanguageEvaluationDuration", 1);
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
  histogram_tester.ExpectTotalCount(
      "Translate.CLD3.TopLanguageEvaluationDuration", 1);
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
  histogram_tester.ExpectTotalCount(
      "Translate.CLD3.TopLanguageEvaluationDuration", 1);
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
  histogram_tester.ExpectTotalCount(
      "Translate.CLD3.TopLanguageEvaluationDuration", 1);
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

}  // namespace
}  // namespace translate
