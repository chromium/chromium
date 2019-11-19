// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_

#include <string>

namespace translate {

namespace TranslateBrowserMetrics {

// An indexing type to query each UMA entry name via GetMetricsName() function.
// Note: |kMetricsEntries| should be updated when a new entry is added here.
enum MetricsNameIndex {
  UMA_INITIATION_STATUS,
  UMA_LANGUAGE_DETECTION_ERROR,
  UMA_LOCALES_ON_DISABLED_BY_PREFS,
  UMA_UNDISPLAYABLE_LANGUAGE,
  UMA_UNSUPPORTED_LANGUAGE_AT_INITIATION,
  UMA_TRANSLATE_SOURCE_LANGUAGE,
  UMA_TRANSLATE_TARGET_LANGUAGE,
  UMA_MAX,
};

// When Chrome Translate is ready to translate a page, one of following reasons
// decides the next browser action.
// Note: Don't insert items. It will change the reporting UMA value and break
// the UMA dashboard page. Instead, append it at the end of enum as suggested
// below. Added items also need to be added to TranslateInitiationStatus in
// enums.xml.
enum InitiationStatusType {
  INITIATION_STATUS_DISABLED_BY_PREFS,
  INITIATION_STATUS_DISABLED_BY_SWITCH,
  INITIATION_STATUS_DISABLED_BY_CONFIG,
  INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED,
  INITIATION_STATUS_URL_IS_NOT_SUPPORTED,
  INITIATION_STATUS_SIMILAR_LANGUAGES,
  INITIATION_STATUS_ACCEPT_LANGUAGES,
  INITIATION_STATUS_AUTO_BY_CONFIG,
  INITIATION_STATUS_AUTO_BY_LINK,
  INITIATION_STATUS_SHOW_INFOBAR,
  INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED,
  INITIATION_STATUS_DISABLED_BY_KEY,
  INITIATION_STATUS_LANGUAGE_IN_ULP,
  INITIATION_STATUS_ABORTED_BY_RANKER,
  INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED,
  INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE,
  INITIATION_STATUS_CREATE_INFOBAR,
  INITIATION_STATUS_SHOW_ICON,
  INITIATION_STATUS_SUPPRESS_INFOBAR,
  INITIATION_STATUS_SHOW_UI_PREDEFINED_TARGET_LANGUAGE,
  INITIATION_STATUS_NO_NETWORK,
  INITIATION_STATUS_DOESNT_NEED_TRANSLATION,
  INITIATION_STATUS_IDENTICAL_LANGUAGE_USE_SOURCE_LANGUAGE_UNKNOWN,
  // Insert new items here.
  INITIATION_STATUS_MAX,
};

// Called when Chrome Translate is initiated to report a reason of the next
// browser action.
void ReportInitiationStatus(InitiationStatusType type);

// Called when Chrome opens the URL so that the user sends an error feedback.
void ReportLanguageDetectionError();

void ReportLocalesOnDisabledByPrefs(const std::string& locale);

// Called when Chrome Translate server sends the language list which includes
// a undisplayable language in the user's locale.
void ReportUndisplayableLanguage(const std::string& language);

void ReportUnsupportedLanguageAtInitiation(const std::string& language);

// Called when a request is sent to the translate server to report the source
// language of the translated page. Buckets are labelled with CLD3LanguageCode
// values.
void ReportTranslateSourceLanguage(const std::string& language);

// Called when a request is sent to the translate server to report the target
// language for the translated page. Buckets are labelled with CLD3LanguageCode
// values.
void ReportTranslateTargetLanguage(const std::string& language);

// Provides UMA entry names for unit tests.
const char* GetMetricsName(MetricsNameIndex index);

}  // namespace TranslateBrowserMetrics

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
