// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_

#include <string>

#include "base/time/time.h"

namespace translate {

// Internals exposed for testing purposes. Should not be relied on by client
// code.
namespace metrics_internal {

// Constant string values to indicate UMA names.
extern const char kTranslateLanguageDetectionLanguageVerification[];
extern const char kTranslateTimeToBeReady[];
extern const char kTranslateTimeToLoad[];
extern const char kTranslateTimeToTranslate[];
extern const char kTranslateUserActionDuration[];
extern const char kTranslatePageScheme[];
extern const char kTranslateSimilarLanguageMatch[];
extern const char kTranslateLanguageDetectionConflict[];
extern const char kTranslateLanguageDeterminedDuration[];
extern const char kTranslatedLanguageDetectionContentLength[];

}  // namespace metrics_internal

// When a valid Content-Language is provided, TranslateAgent checks if a
// server provided Content-Language matches to a language the model determined.
// This enum is used for recording metrics. This enum should remain synchronized
// with the enum "TranslateLanguageVerification" in enums.xml.
enum LanguageVerificationType {
  DEPRECATED_LANGUAGE_VERIFICATION_MODEL_DISABLED,  // obsolete
  LANGUAGE_VERIFICATION_MODEL_ONLY,
  LANGUAGE_VERIFICATION_UNKNOWN,
  LANGUAGE_VERIFICATION_MODEL_AGREE,
  LANGUAGE_VERIFICATION_MODEL_DISAGREE,
  LANGUAGE_VERIFICATION_TRUST_MODEL,
  LANGUAGE_VERIFICATION_MODEL_COMPLEMENT_SUB_CODE,
  LANGUAGE_VERIFICATION_MAX,
};

// Scheme type of pages Chrome is going to translate.
enum SchemeType {
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_OTHERS,
  SCHEME_MAX,
};

// Called when CLD verifies Content-Language header.
void ReportLanguageVerification(LanguageVerificationType type);

// Called when the Translate Element library is ready.
void ReportTimeToBeReady(double time_in_msec);

// Called when the Translate Element library is loaded.
void ReportTimeToLoad(double time_in_msec);

// Called when a page translation is finished.
void ReportTimeToTranslate(double time_in_msec);

// Called when a translation is triggered.
void ReportUserActionDuration(base::TimeTicks begin, base::TimeTicks end);

// Called when a translation is triggered.
void ReportPageScheme(const std::string& scheme);

// Called when CLD agreed on a language which is different, but in the similar
// language list.
void ReportSimilarLanguageMatch(bool match);

// Called when the page language is determined.
void ReportLanguageDeterminedDuration(base::TimeTicks begin,
                                      base::TimeTicks end);

// Called after when a translation starts.
void ReportTranslatedLanguageDetectionContentLength(size_t content_length);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
