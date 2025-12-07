// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_

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
extern const char kTranslateCompactInfobarEvent[];

}  // namespace metrics_internal

// When a valid Content-Language is provided, TranslateAgent checks if a
// server provided Content-Language matches to a language the model determined.
// This enum is used for recording metrics. This enum should remain synchronized
// with the enum "TranslateLanguageVerification" in enums.xml.
enum class LanguageVerificationType {
  // kModelDisabled = 0, -- obsolete
  kModelOnly = 1,
  kModelUnknown = 2,
  kModelAgrees = 3,
  kModelDisagrees = 4,
  kModelOverrides = 5,
  kModelComplementsCountry = 6,
  kNoPageContent = 7,
  kModelNotAvailable = 8,
  kModelHistogramBoundary = 9,
  kMaxValue = kModelHistogramBoundary,
};

// Enum for the Translate.CompactInfobar.Event UMA histogram.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.infobar
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: InfobarEvent
enum class InfobarEvent {
  INFOBAR_IMPRESSION = 0,
  INFOBAR_TARGET_TAB_TRANSLATE = 1,
  INFOBAR_DECLINE = 2,
  INFOBAR_OPTIONS = 3,
  INFOBAR_MORE_LANGUAGES = 4,
  INFOBAR_MORE_LANGUAGES_TRANSLATE = 5,
  INFOBAR_PAGE_NOT_IN = 6,
  INFOBAR_ALWAYS_TRANSLATE = 7,
  INFOBAR_NEVER_TRANSLATE = 8,
  INFOBAR_NEVER_TRANSLATE_SITE = 9,
  INFOBAR_SCROLL_HIDE = 10,
  INFOBAR_SCROLL_SHOW = 11,
  INFOBAR_REVERT = 12,
  INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION = 13,
  INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION = 14,
  INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION = 15,
  INFOBAR_SNACKBAR_CANCEL_ALWAYS = 16,
  INFOBAR_SNACKBAR_CANCEL_NEVER_SITE = 17,
  INFOBAR_SNACKBAR_CANCEL_NEVER = 18,
  INFOBAR_ALWAYS_TRANSLATE_UNDO = 19,
  INFOBAR_CLOSE_DEPRECATED = 20,
  INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION = 21,
  INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION = 22,
  INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS = 23,
  INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER = 24,
  // 25 was a duplicate code and is now deprecated https://crbug.com/1414604
  INFOBAR_NEVER_TRANSLATE_UNDO = 26,
  INFOBAR_NEVER_TRANSLATE_SITE_UNDO = 27,
  INFOBAR_HISTOGRAM_BOUNDARY = 28,
  kMaxValue = INFOBAR_HISTOGRAM_BOUNDARY,
};

// Called when CLD verifies Content-Language header.
void ReportLanguageVerification(LanguageVerificationType type);

// Called when the Translate Element library is ready.
void ReportTimeToBeReady(double time_in_msec);

// Called when the Translate Element library is loaded.
void ReportTimeToLoad(double time_in_msec);

// Called when a page translation is finished.
void ReportTimeToTranslate(double time_in_msec);

// Called when the page language is determined.
void ReportLanguageDeterminedDuration(base::TimeTicks begin,
                                      base::TimeTicks end);

// Called after when a translation starts.
void ReportTranslatedLanguageDetectionContentLength(size_t content_length);

// Called when the Android Messages or iOS Translate UI is shown.
void ReportCompactInfobarEvent(InfobarEvent event);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
