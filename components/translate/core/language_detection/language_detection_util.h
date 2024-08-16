// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_UTIL_H_

#include <string>

namespace translate {
enum class LanguageVerificationType;

// Given a detected language and whether that detection is reliable, returns the
// ISO 639 language code of |utf8_text|. Returns
// |translate::kUnknownLanguageCode|
//  for unreliable, "unknown", and xx-Latn predictions that are currently not
// supported.
std::string FilterDetectedLanguage(const std::string& utf8_text,
                                   const std::string& detected_language,
                                   bool is_detection_reliable);

// Returns the ISO 639 language code of the specified |utf8_text|, or
// |translate::kUnknownLanguageCode| if it failed. |is_model_reliable| will be
// set as true if CLD says the detection is reliable and
// |model_reliability_score| will contain the model's confidence in that
// detection.
std::string DetermineTextLanguage(const std::string& utf8_text,
                                  bool* is_model_reliable,
                                  float& model_reliability_score);

// Determines page language from content header and html lang when no model is
// available.
std::string DeterminePageLanguageNoModel(
    const std::string& content_lang,
    const std::string& html_lang,
    translate::LanguageVerificationType language_verification_type);

// Determines page language from content header, html lang and contents.
// Returns the contents language results in |model_detected_language| and
// |is_model_reliable| and the model's confidence it its detection language
// in |model_reliability_score|.
std::string DeterminePageLanguage(const std::string& code,
                                  const std::string& html_lang,
                                  const std::u16string& contents,
                                  std::string* model_detected_language,
                                  bool* is_model_reliable,
                                  float& model_reliability_score);

// Determines content page language from Content-Language code and contents
// language.
std::string DeterminePageLanguage(const std::string& code,
                                  const std::string& html_lang,
                                  const std::string& model_detected_language,
                                  bool is_model_reliable);

// Corrects language code if it contains well-known mistakes.
// Called only by tests.
void CorrectLanguageCodeTypo(std::string* code);

// Checks if the language code's format is valid.
// Called only by tests.
bool IsValidLanguageCode(const std::string& code);

// Checks if languages are matched, or similar. This function returns true
// against a language pair containing a language which is difficult for CLD to
// distinguish.
// Called only by tests.
bool IsSameOrSimilarLanguages(const std::string& page_language,
                              const std::string& model_detected_language);

// Checks if languages pair is one of well-known pairs of wrong server
// configuration.
// Called only by tests.
bool MaybeServerWrongConfiguration(const std::string& page_language,
                                   const std::string& model_detected_language);

// Returns true if the specified language often has the wrong server
// configuration language, false otherwise.
bool IsServerWrongConfigurationLanguage(const std::string& language);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_UTIL_H_
