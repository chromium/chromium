// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_util.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
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
  for (size_t i = 0; i < base::size(kSimilarLanguageCodes); ++i) {
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

// Returns the ISO 639 language code of the specified |text|, or 'unknown' if it
// failed.
// |is_cld_reliable| will be set as true if CLD says the detection is reliable.
std::string DetermineTextLanguage(const base::string16& text,
                                  bool* is_cld_reliable) {
  std::string language = translate::kUnknownLanguageCode;
  const std::string utf8_text(base::UTF16ToUTF8(text));

  // Make a prediction.
  chrome_lang_id::NNetLanguageIdentifier lang_id;
  const chrome_lang_id::NNetLanguageIdentifier::Result lang_id_result =
      lang_id.FindTopNMostFreqLangs(utf8_text, /*num_langs=*/1).at(0);
  const bool prediction_reliable = lang_id_result.is_reliable;
  const std::string& predicted_language = lang_id_result.language;

  // Update histograms.
  const base::HistogramBase::Sample pred_lang_hash =
      static_cast<base::HistogramBase::Sample>(
          base::HashMetricName(predicted_language));
  base::UmaHistogramSparse("Translate.CLD3.LanguageDetected", pred_lang_hash);
  if (predicted_language != chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
    UMA_HISTOGRAM_PERCENTAGE("Translate.CLD3.LanguagePercentage",
                             static_cast<int>(100 * lang_id_result.proportion));
  }

  if (is_cld_reliable != nullptr) {
    *is_cld_reliable = prediction_reliable;
  }

  // Ignore unreliable, "unknown", and xx-Latn predictions that are currently
  // not supported.
  if (prediction_reliable &&
      predicted_language != "bg-Latn" &&
      predicted_language != "el-Latn" &&
      predicted_language != "ja-Latn" &&
      predicted_language != "ru-Latn" &&
      predicted_language != "zh-Latn" &&
      predicted_language !=
          chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
    if (predicted_language != "zh") {
      language = predicted_language;
    } else {
      // If prediction is "zh" (Chinese), then we need to determine whether the
      // text is zh-Hant (Chinese Traditional) or zh-Hans (Chinese Simplified).
      translate::ChineseScriptClassifier zh_classifier;

      // The Classify function returns either "zh-Hant" or "zh-Hans".
      // Convert to the old-style language codes used by the Translate API.
      const std::string zh_classification = zh_classifier.Classify(utf8_text);
      if (zh_classification == "zh-Hant") {
        language = "zh-TW";
      } else if (zh_classification == "zh-Hans") {
        language = "zh-CN";
      } else {
        language = translate::kUnknownLanguageCode;
      }
    }
  }

  VLOG(1) << "Detected language: " << language;
  return language;
}

// Checks if CLD can complement a sub code when the page language doesn't know
// the sub code.
bool CanCLDComplementSubCode(
    const std::string& page_language, const std::string& cld_language) {
  // Translate server cannot treat general Chinese. If Content-Language and
  // CLD agree that the language is Chinese and Content-Language doesn't know
  // which dialect is used, CLD language has priority.
  // TODO(hajimehoshi): How about the other dialects like zh-MO?
  return page_language == "zh" &&
         base::StartsWith(cld_language, "zh-",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

namespace translate {

std::string DeterminePageLanguage(const std::string& code,
                                  const std::string& html_lang,
                                  const base::string16& contents,
                                  std::string* cld_language_p,
                                  bool* is_cld_reliable_p) {
  bool is_cld_reliable;
  // Check if html lang attribute is valid.
  std::string modified_html_lang;
  if (!html_lang.empty()) {
    modified_html_lang = html_lang;
    ApplyLanguageCodeCorrection(&modified_html_lang);
    translate::ReportHtmlLang(html_lang, modified_html_lang);
    VLOG(9) << "html lang based language code: " << modified_html_lang;
  }

  // Check if Content-Language is valid.
  std::string modified_code;
  if (!code.empty()) {
    modified_code = code;
    ApplyLanguageCodeCorrection(&modified_code);
    translate::ReportContentLanguage(code, modified_code);
  }

  std::string cld_language = DetermineTextLanguage(contents, &is_cld_reliable);

  if (cld_language_p != nullptr)
    *cld_language_p = cld_language;
  if (is_cld_reliable_p != nullptr)
    *is_cld_reliable_p = is_cld_reliable;
  language::ToTranslateLanguageSynonym(&cld_language);

  // Adopt |modified_html_lang| if it is valid. Otherwise, adopt
  // |modified_code|.
  std::string language = modified_html_lang.empty() ? modified_code :
                                                      modified_html_lang;

  // When the page language is English, log conflicting CLD results. We will use
  // these metrics to decide when to favor CLD.
  if (language.substr(0, 2) == "en" && cld_language.substr(0, 2) != "en" &&
      cld_language != kUnknownLanguageCode) {
    translate::ReportLanguageDetectionConflict(language, cld_language);
  }

  // If |language| is empty, just use CLD result even though it might be
  // translate::kUnknownLanguageCode.
  if (language.empty()) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_CLD_ONLY);
    return cld_language;
  }

  if (cld_language == kUnknownLanguageCode) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_UNKNOWN);
    return language;
  }

  if (CanCLDComplementSubCode(language, cld_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE);
    return cld_language;
  }

  if (IsSameOrSimilarLanguages(language, cld_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_CLD_AGREE);
    return language;
  }

  if (MaybeServerWrongConfiguration(language, cld_language)) {
    translate::ReportLanguageVerification(
        translate::LANGUAGE_VERIFICATION_TRUST_CLD);
    return cld_language;
  }

  // Content-Language value might be wrong because CLD says that this page is
  // written in another language with confidence. In this case, Chrome doesn't
  // rely on any of the language codes, and gives up suggesting a translation.
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_CLD_DISAGREE);
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
                              const std::string& cld_language) {
  std::vector<std::string> chunks = base::SplitString(
      page_language, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.size() == 0)
    return false;
  std::string page_language_main_part = chunks[0];  // Need copy.

  chunks = base::SplitString(
      cld_language, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.size() == 0)
    return false;
  const std::string& cld_language_main_part = chunks[0];

  // Language code part of |page_language| is matched to one of |cld_language|.
  // Country code is ignored here.
  if (page_language_main_part == cld_language_main_part) {
    // Languages are matched strictly. Reports false to metrics, but returns
    // true.
    translate::ReportSimilarLanguageMatch(false);
    return true;
  }

  // Check if |page_language| and |cld_language| are in the similar language
  // list and belong to the same language group.
  int page_code = GetSimilarLanguageGroupCode(page_language);
  bool match = page_code != 0 &&
               page_code == GetSimilarLanguageGroupCode(cld_language);

  translate::ReportSimilarLanguageMatch(match);
  return match;
}

bool IsServerWrongConfigurationLanguage(const std::string& language_code) {
  for (size_t i = 0; i < base::size(kWellKnownCodesOnWrongConfiguration); ++i) {
    if (language_code == kWellKnownCodesOnWrongConfiguration[i])
      return true;
  }
  return false;
}

bool MaybeServerWrongConfiguration(const std::string& page_language,
                                   const std::string& cld_language) {
  // If |page_language| is not "en-*", respect it and just return false here.
  if (!base::StartsWith(page_language, "en",
                        base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // A server provides a language meta information representing "en-*". But it
  // might be just a default value due to missing user configuration.
  // Let's trust |cld_language| if the determined language is not difficult to
  // distinguish from English, and the language is one of well-known languages
  // which often provide "en-*" meta information mistakenly.
  return IsServerWrongConfigurationLanguage(cld_language);
}

}  // namespace translate
