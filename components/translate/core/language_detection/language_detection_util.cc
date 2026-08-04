// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_util.h"

#include <stddef.h>

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language_detection/core/chinese_script_classifier.h"
#include "components/language_detection/core/constants.h"
#include "components/translate/core/common/translate_language_matcher.h"
#include "components/translate/core/common/translate_metrics.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"

namespace translate {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagConverter;

constexpr auto kBosnianCroatian = base::MakeFixedFlatSet<LanguageTag>(
    {GetKnownLanguageTag("bs"), GetKnownLanguageTag("hr")});
constexpr auto kHindiNepali = base::MakeFixedFlatSet<LanguageTag>(
    {GetKnownLanguageTag("hi"), GetKnownLanguageTag("ne")});

bool IsSameOrSimilarLanguages(std::string_view lhs, std::string_view rhs) {
  std::optional<LanguageTag> first_lang =
      LanguageTagConverter::GetInstance().FromString(lhs);
  std::optional<LanguageTag> second_lang =
      LanguageTagConverter::GetInstance().FromString(rhs);
  if (!first_lang || !second_lang) {
    return false;
  }
  LanguageTag first_language_subtag_only = first_lang->WithLanguageSubtagOnly();
  LanguageTag second_language_subtag_only =
      second_lang->WithLanguageSubtagOnly();

  return first_language_subtag_only == second_language_subtag_only ||
         (kBosnianCroatian.contains(first_language_subtag_only) &&
          kBosnianCroatian.contains(second_language_subtag_only)) ||
         (kHindiNepali.contains(first_language_subtag_only) &&
          kHindiNepali.contains(second_language_subtag_only));
}

std::optional<LanguageTag> GetTranslateLanguage(
    std::optional<LanguageTag> tag) {
  if (!tag) {
    return std::nullopt;
  }
  return GetTranslateLanguageMatcher().Match(*tag);
}

// Get page language from html language code if it is not empty, otherwise get
// page language from Content-Language code. It returns nullopt when none are
// valid.
std::optional<LanguageTag> GetHTMLOrHTTPContentLanguage(
    std::string_view content_lang,
    std::string_view html_lang) {
  // Check if html lang attribute is valid.
  std::optional<LanguageTag> html_lang_tag = GetTranslateLanguage(
      LanguageTagConverter::GetInstance().FromString(html_lang));
  if (html_lang_tag) {
    return *html_lang_tag;
  }

  return GetTranslateLanguage(
      LanguageTagConverter::GetInstance().FromString(content_lang));
}

// Checks if the model can complement a sub code when the page language doesn't
// know the sub code.
bool CanModelComplementSubCode(std::string_view page_language,
                               std::string_view model_detected_language) {
  // Translate server cannot treat general Chinese. If Content-Language and
  // the detection model agree that the language is Chinese and Content-Language
  // doesn't know which dialect is used, the model language has priority.
  // TODO(hajimehoshi): How about the other dialects like zh-MO?
  return page_language == "zh" &&
         base::StartsWith(model_detected_language, "zh-",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

std::optional<LanguageTag> FilterDetectedLanguage(
    const std::string& utf8_text,
    const std::string& detected_language,
    bool is_detection_reliable) {
  // Ignore unreliable, "unknown", and xx-Latn predictions that are currently
  // not supported.
  if (!is_detection_reliable)
    return std::nullopt;
  // TODO(crbug.com/40169055): Determine if ar-Latn and hi-Latn need to be added
  // for the TFLite-based detection model.
  if (detected_language == "bg-Latn" || detected_language == "el-Latn" ||
      detected_language == "ja-Latn" || detected_language == "ru-Latn" ||
      detected_language == "zh-Latn" ||
      detected_language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
    return std::nullopt;
  }

  if (detected_language == "zh") {
    // If prediction is "zh" (Chinese), then we need to determine whether the
    // text is zh-Hant (Chinese Traditional) or zh-Hans (Chinese Simplified).
    language_detection::ChineseScriptClassifier zh_classifier;

    // The Classify function returns either "zh-Hant" or "zh-Hans".
    // Convert to the old-style language codes used by the Translate API.
    const std::string zh_classification = zh_classifier.Classify(utf8_text);
    if (zh_classification == "zh-Hant")
      return GetKnownLanguageTag("zh-TW");
    if (zh_classification == "zh-Hans")
      return GetKnownLanguageTag("zh-CN");
    return std::nullopt;
  }
  // The detection is reliable and none of the cases that are not handled by the
  // language detection model.
  return LanguageTagConverter::GetInstance().FromString(detected_language);
}

// Returns the ISO 639 language code of the specified |utf8_text|, or 'unknown'
// if it failed. |is_model_reliable| will be set as true if CLD says the
// detection is reliable and |model_reliability_score| will provide the model's
// confidence in that prediction.
std::optional<LanguageTag> DetermineTextLanguage(
    const std::string& utf8_text,
    bool* is_model_reliable,
    float& model_reliability_score) {
  // Make a prediction.
  chrome_lang_id::NNetLanguageIdentifier lang_id;
  const chrome_lang_id::NNetLanguageIdentifier::Result lang_id_result =
      lang_id.FindTopNMostFreqLangs(utf8_text, /*num_langs=*/1).at(0);
  const bool is_detection_reliable = lang_id_result.is_reliable;
  const float model_probability = lang_id_result.probability;
  const std::string& detected_language = lang_id_result.language;

  // Update histograms.
  const base::HistogramBase::Sample32 pred_lang_hash =
      static_cast<base::HistogramBase::Sample32>(
          base::HashMetricName(detected_language));
  base::UmaHistogramSparse("Translate.CLD3.LanguageDetected", pred_lang_hash);
  if (detected_language != chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
    UMA_HISTOGRAM_PERCENTAGE("Translate.CLD3.LanguagePercentage",
                             static_cast<int>(100 * lang_id_result.proportion));
  }

  if (is_model_reliable != nullptr) {
    *is_model_reliable = is_detection_reliable;
  }
  model_reliability_score = model_probability;
  return FilterDetectedLanguage(utf8_text, detected_language,
                                is_detection_reliable);
}

std::string DeterminePageLanguage(std::string_view code,
                                  std::string_view html_lang,
                                  const std::u16string& contents,
                                  std::string* model_detected_language,
                                  bool* is_model_reliable,
                                  float& model_reliability_score) {
  // First determine the language for the text contents.
  bool is_reliable;
  float model_score = 0.0;
  const std::string utf8_text(base::UTF16ToUTF8(contents));
  LanguageTag detected_language_tag =
      DetermineTextLanguage(utf8_text, &is_reliable, model_score)
          .value_or(GetKnownLanguageTag("und"));
  if (model_detected_language != nullptr) {
    *model_detected_language = std::string(detected_language_tag.tag_string());
  }
  if (is_model_reliable != nullptr)
    *is_model_reliable = is_reliable;
  model_reliability_score = model_score;
  LanguageTag translate_detected_tag =
      GetTranslateLanguageMatcher()
          .Match(detected_language_tag)
          .value_or(GetKnownLanguageTag("und"));

  return DeterminePageLanguage(
      code, html_lang, translate_detected_tag.tag_string(), is_reliable);
}

std::string DeterminePageLanguageNoModel(
    std::string_view code,
    std::string_view html_lang,
    LanguageVerificationType language_verification_type) {
  translate::ReportLanguageVerification(language_verification_type);
  std::optional<LanguageTag> language =
      GetHTMLOrHTTPContentLanguage(code, html_lang);
  return !language.has_value() ? language_detection::kUnknownLanguageCode
                               : std::string(language->tag_string());
}

// Now consider the web page language details along with the contents language.
std::string DeterminePageLanguage(std::string_view code,
                                  std::string_view html_lang,
                                  std::string_view model_detected_language,
                                  bool is_model_reliable) {
  std::optional<LanguageTag> language =
      GetHTMLOrHTTPContentLanguage(code, html_lang);
  // If |language| is nullopt, just use model result even though it might be
  // language_detection::kUnknownLanguageCode.
  if (!language.has_value()) {
    translate::ReportLanguageVerification(
        translate::LanguageVerificationType::kModelOnly);
    return std::string(model_detected_language);
  }

  // If |model_detected_language| is empty, just use |language|.
  if (model_detected_language.empty() ||
      model_detected_language == language_detection::kUnknownLanguageCode) {
    translate::ReportLanguageVerification(
        translate::LanguageVerificationType::kModelUnknown);
    return std::string(language->tag_string());
  }

  if (CanModelComplementSubCode(language->tag_string(),
                                model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LanguageVerificationType::kModelComplementsCountry);
    return std::string(model_detected_language);
  }

  if (IsSameOrSimilarLanguages(language->tag_string(),
                               model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LanguageVerificationType::kModelAgrees);
    return std::string(language->tag_string());
  }

  if (MaybeServerWrongConfiguration(language->tag_string(),
                                    model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LanguageVerificationType::kModelOverrides);
    return std::string(model_detected_language);
  }

  // Content-Language value might be wrong because model says that this page is
  // written in another language with confidence. In this case, Chrome doesn't
  // rely on any of the language codes, and gives up suggesting a translation.
  translate::ReportLanguageVerification(
      translate::LanguageVerificationType::kModelDisagrees);
  return language_detection::kUnknownLanguageCode;
}

void CorrectLanguageCodeTypo(std::string* code) {
  DCHECK(code);

  size_t coma_index = code->find(',');
  if (coma_index != std::string::npos) {
    // There are more than 1 language specified, just keep the first one.
    *code = code->substr(0, coma_index);
  }
  base::TrimWhitespaceASCII(*code, base::TRIM_ALL, code);

  // An underscore instead of a dash is a frequent mistake.
  size_t underscore_index = code->find('_');
  if (underscore_index != std::string::npos)
    (*code)[underscore_index] = '-';

  // Change everything up to a dash to lower-case and everything after to upper.
  size_t dash_index = code->find('-');
  if (dash_index != std::string::npos) {
    *code = base::ToLowerASCII(code->substr(0, dash_index)) +
        base::ToUpperASCII(code->substr(dash_index));
  } else {
    *code = base::ToLowerASCII(*code);
  }
}

bool IsValidLanguageCode(std::string_view code) {
  // Roughly check if the language code follows /[a-zA-Z]{2,3}(-[a-zA-Z]{2})?/.
  // TODO(hajimehoshi): How about es-419, which is used as an Accept language?
  std::vector<std::string_view> chunks = base::SplitStringPiece(
      code, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (chunks.size() < 1 || 2 < chunks.size())
    return false;

  std::string_view main_code = chunks[0];

  if (main_code.size() < 1 || 3 < main_code.size())
    return false;

  for (char c : main_code) {
    if (!base::IsAsciiAlpha(c))
      return false;
  }

  if (chunks.size() == 1)
    return true;

  std::string_view sub_code = chunks[1];

  if (sub_code.size() != 2)
    return false;

  for (char c : sub_code) {
    if (!base::IsAsciiAlpha(c))
      return false;
  }

  return true;
}

bool IsServerWrongConfigurationLanguage(std::string_view language_code) {
  // Well-known languages which often have wrong server configuration of
  // Content-Language: en.
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>(
      {"es", "pt", "ja", "ru", "de", "zh-CN", "zh-TW", "ar", "id", "fr", "it",
       "th"});

  return kSet.contains(language_code);
}

bool MaybeServerWrongConfiguration(std::string_view page_language,
                                   std::string_view model_detected_language) {
  // If |page_language| is not "en-*", respect it and just return false here.
  if (!base::StartsWith(page_language, "en",
                        base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // A server provides a language meta information representing "en-*". But it
  // might be just a default value due to missing user configuration.
  // Let's trust |model_detected_language| if the determined language is not
  // difficult to distinguish from English, and the language is one of
  // well-known languages which often provide "en-*" meta information
  // mistakenly.
  return IsServerWrongConfigurationLanguage(model_detected_language);
}

}  // namespace translate
