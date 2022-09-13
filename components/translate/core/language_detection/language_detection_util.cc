// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_util.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/core/common/language_util.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/language_detection/chinese_script_classifier.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"

namespace {

// Similar language code list. Some languages are very similar and difficult
// for CLD to distinguish.
struct SimilarLanguageCode {
  const char* const code;
  int group;
};

const SimilarLanguageCode kSimilarLanguageCodes[] = {
  {"bs", 1},
  {"hr", 1},
  {"hi", 2},
  {"ne", 2},
};

// Checks |kSimilarLanguageCodes| and returns group code.
int GetSimilarLanguageGroupCode(const std::string& language) {
  for (size_t i = 0; i < std::size(kSimilarLanguageCodes); ++i) {
    if (language.find(kSimilarLanguageCodes[i].code) != 0)
      continue;
    return kSimilarLanguageCodes[i].group;
  }
  return 0;
}

// Well-known languages which often have wrong server configuration of
// Content-Language: en.
const char* const kWellKnownCodesOnWrongConfiguration[] = {
    "es",    "pt", "ja", "ru", "de", "zh-CN",
    "zh-TW", "ar", "id", "fr", "it", "th"};

// Applies a series of language code modification in proper order.
void ApplyLanguageCodeCorrection(std::string* code) {
  // Correct well-known format errors.
  translate::CorrectLanguageCodeTypo(code);

  if (!translate::IsValidLanguageCode(*code)) {
    *code = std::string();
    return;
  }

  language::ToTranslateLanguageSynonym(code);
}

// Checks if the model can complement a sub code when the page language doesn't
// know the sub code.
bool CanModelComplementSubCode(const std::string& page_language,
                               const std::string& model_detected_language) {
  // Translate server cannot treat general Chinese. If Content-Language and
  // the detection model agree that the language is Chinese and Content-Language
  // doesn't know which dialect is used, the model language has priority.
  // TODO(hajimehoshi): How about the other dialects like zh-MO?
  return page_language == "zh" &&
         base::StartsWith(model_detected_language, "zh-",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

namespace translate {

std::string FilterDetectedLanguage(const std::string& utf8_text,
                                   const std::string& detected_language,
                                   bool is_detection_reliable) {
  // Ignore unreliable, "unknown", and xx-Latn predictions that are currently
  // not supported.
  if (!is_detection_reliable)
    return translate::kUnknownLanguageCode;
  // TODO(crbug.com/1178193): Determine if ar-Latn and hi-Latn need to be added
  // for the TFLite-based detection model.
  if (detected_language == "bg-Latn" || detected_language == "el-Latn" ||
      detected_language == "ja-Latn" || detected_language == "ru-Latn" ||
      detected_language == "zh-Latn" ||
      detected_language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
    return translate::kUnknownLanguageCode;
  }

  if (detected_language == "zh") {
    // If prediction is "zh" (Chinese), then we need to determine whether the
    // text is zh-Hant (Chinese Traditional) or zh-Hans (Chinese Simplified).
    translate::ChineseScriptClassifier zh_classifier;

    // The Classify function returns either "zh-Hant" or "zh-Hans".
    // Convert to the old-style language codes used by the Translate API.
    const std::string zh_classification = zh_classifier.Classify(utf8_text);
    if (zh_classification == "zh-Hant")
      return "zh-TW";
    if (zh_classification == "zh-Hans")
      return "zh-CN";
    return translate::kUnknownLanguageCode;
  }
  // The detection is reliable and none of the cases that are not handled by the
  // language detection model.
  return detected_language;
}

// Returns the ISO 639 language code of the specified |utf8_text|, or 'unknown'
// if it failed. |is_model_reliable| will be set as true if CLD says the
// detection is reliable and |model_reliability_score| will provide the model's
// confidence in that prediction.
std::string DetermineTextLanguage(const std::string& utf8_text,
                                  bool* is_model_reliable,
                                  float& model_reliability_score) {
  // Make a prediction.
  base::TimeTicks lang_id_start = base::TimeTicks::Now();
  chrome_lang_id::NNetLanguageIdentifier lang_id;
  const chrome_lang_id::NNetLanguageIdentifier::Result lang_id_result =
      lang_id.FindTopNMostFreqLangs(utf8_text, /*num_langs=*/1).at(0);
  base::UmaHistogramTimes("Translate.CLD3.TopLanguageEvaluationDuration",
                          base::TimeTicks::Now() - lang_id_start);
  const bool is_detection_reliable = lang_id_result.is_reliable;
  const float model_probability = lang_id_result.probability;
  const std::string& detected_language = lang_id_result.language;

  // Update histograms.
  const base::HistogramBase::Sample pred_lang_hash =
      static_cast<base::HistogramBase::Sample>(
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

std::string DeterminePageLanguage(const std::string& code,
                                  const std::string& html_lang,
                                  const std::u16string& contents,
                                  std::string* model_detected_language,
                                  bool* is_model_reliable,
                                  float& model_reliability_score) {
  // First determine the language for the text contents.
  bool is_reliable;
  float model_score = 0.0;
  const std::string utf8_text(base::UTF16ToUTF8(contents));
  std::string detected_language =
      DetermineTextLanguage(utf8_text, &is_reliable, model_score);
  if (model_detected_language != nullptr)
    *model_detected_language = detected_language;
  if (is_model_reliable != nullptr)
    *is_model_reliable = is_reliable;
  model_reliability_score = model_score;
  language::ToTranslateLanguageSynonym(&detected_language);

  return DeterminePageLanguage(code, html_lang, detected_language, is_reliable);
}

// Now consider the web page language details along with the contents language.
std::string DeterminePageLanguage(const std::string& code,
                                  const std::string& html_lang,
                                  const std::string& model_detected_language,
                                  bool is_model_reliable) {
  // Check if html lang attribute is valid.
  std::string modified_html_lang;
  if (!html_lang.empty()) {
    modified_html_lang = html_lang;
    ApplyLanguageCodeCorrection(&modified_html_lang);
    VLOG(9) << "html lang based language code: " << modified_html_lang;
  }

  // Check if Content-Language is valid.
  std::string modified_code;
  if (!code.empty()) {
    modified_code = code;
    ApplyLanguageCodeCorrection(&modified_code);
  }

  // Adopt |modified_html_lang| if it is valid. Otherwise, adopt
  // |modified_code|.
  std::string language = modified_html_lang.empty() ? modified_code :
                                                      modified_html_lang;

  // If |language| is empty, just use model result even though it might be
  // translate::kUnknownLanguageCode.
  if (language.empty()) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_MODEL_ONLY);
    return model_detected_language;
  }

  if (model_detected_language == kUnknownLanguageCode) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_UNKNOWN);
    return language;
  }

  if (CanModelComplementSubCode(language, model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_MODEL_COMPLEMENT_SUB_CODE);
    return model_detected_language;
  }

  if (IsSameOrSimilarLanguages(language, model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_MODEL_AGREE);
    return language;
  }

  if (MaybeServerWrongConfiguration(language, model_detected_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_TRUST_MODEL);
    return model_detected_language;
  }

  // Content-Language value might be wrong because model says that this page is
  // written in another language with confidence. In this case, Chrome doesn't
  // rely on any of the language codes, and gives up suggesting a translation.
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_MODEL_DISAGREE);
  return kUnknownLanguageCode;
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

bool IsValidLanguageCode(const std::string& code) {
  // Roughly check if the language code follows /[a-zA-Z]{2,3}(-[a-zA-Z]{2})?/.
  // TODO(hajimehoshi): How about es-419, which is used as an Accept language?
  std::vector<base::StringPiece> chunks = base::SplitStringPiece(
      code, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (chunks.size() < 1 || 2 < chunks.size())
    return false;

  const base::StringPiece& main_code = chunks[0];

  if (main_code.size() < 1 || 3 < main_code.size())
    return false;

  for (char c : main_code) {
    if (!base::IsAsciiAlpha(c))
      return false;
  }

  if (chunks.size() == 1)
    return true;

  const base::StringPiece& sub_code = chunks[1];

  if (sub_code.size() != 2)
    return false;

  for (char c : sub_code) {
    if (!base::IsAsciiAlpha(c))
      return false;
  }

  return true;
}

bool IsSameOrSimilarLanguages(const std::string& page_language,
                              const std::string& model_detected_language) {
  std::vector<std::string> chunks = base::SplitString(
      page_language, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.size() == 0)
    return false;
  std::string page_language_main_part = chunks[0];  // Need copy.

  chunks = base::SplitString(model_detected_language, "-",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.size() == 0)
    return false;
  const std::string& model_detected_language_main_part = chunks[0];

  // Language code part of |page_language| is matched to one of
  // |model_detected_language|. Country code is ignored here.
  if (page_language_main_part == model_detected_language_main_part) {
    // Languages are matched strictly. Reports false to metrics, but returns
    // true.
    translate::ReportSimilarLanguageMatch(false);
    return true;
  }

  // Check if |page_language| and |model_detected_language| are in the similar
  // language list and belong to the same language group.
  int page_code = GetSimilarLanguageGroupCode(page_language);
  bool match = page_code != 0 && page_code == GetSimilarLanguageGroupCode(
                                                  model_detected_language);

  translate::ReportSimilarLanguageMatch(match);
  return match;
}

bool IsServerWrongConfigurationLanguage(const std::string& language_code) {
  for (size_t i = 0; i < std::size(kWellKnownCodesOnWrongConfiguration); ++i) {
    if (language_code == kWellKnownCodesOnWrongConfiguration[i])
      return true;
  }
  return false;
}

bool MaybeServerWrongConfiguration(const std::string& page_language,
                                   const std::string& model_detected_language) {
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
