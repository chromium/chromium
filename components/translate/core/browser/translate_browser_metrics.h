// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_

#include <stddef.h>

#include "base/strings/string_piece.h"

namespace translate {

namespace TranslateBrowserMetrics {

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

enum class HrefTranslateStatus {
  kAutoTranslated,
  kAutoTranslatedDifferentTargetLanguage,

  // Deprecated, use the below values instead.
  kDeprecatedNotAutoTranslated,

  kUiShownNotAutoTranslated,
  kNoUiShownNotAutoTranslated,

  // Insert new items here. Keep in sync with HrefTranslateStatus in enums.xml
  // when adding values.
  kMaxValue = kNoUiShownNotAutoTranslated
};

enum class HrefTranslatePrefsFilterStatus {
  kNotInBlocklists,
  kLanguageInBlocklist,
  kSiteInBlocklist,
  kBothLanguageAndSiteInBlocklist,

  // Insert new items here. Keep in sync with HrefTranslatePrefsFilterStatus in
  // enums.xml when adding values.
  kMaxValue = kBothLanguageAndSiteInBlocklist
};

enum class TargetLanguageOrigin {
  kRecentTarget,
  kLanguageModel,
  kApplicationUI,
  kAcceptLanguages,
  kDefaultEnglish,
  // Insert new items here. Keep in sync with TranslateTargetLanguageOrigin in
  // enums.xml when adding values.
  kMaxValue = kDefaultEnglish
};

// Called when Chrome Translate is initiated to report a reason of the next
// browser action.
void ReportInitiationStatus(InitiationStatusType type);

// Called when Chrome opens the URL so that the user sends an error feedback.
void ReportLanguageDetectionError();

// Called when language detection details are complete.
void ReportLanguageDetectionContentLength(size_t length);

void ReportLocalesOnDisabledByPrefs(base::StringPiece locale);

// Called when Chrome Translate server sends the language list which includes
// a undisplayable language in the user's locale.
void ReportUndisplayableLanguage(base::StringPiece language);

void ReportUnsupportedLanguageAtInitiation(base::StringPiece language);

// Called when a request is sent to the translate server to report the source
// language of the translated page. Buckets are labelled with CLD3LanguageCode
// values.
void ReportTranslateSourceLanguage(base::StringPiece language);

// Called when a request is sent to the translate server to report the target
// language for the translated page. Buckets are labelled with CLD3LanguageCode
// values.
void ReportTranslateTargetLanguage(base::StringPiece language);

// Called when Chrome Translate is initiated, the navigation is from Google, and
// a href translate target is present.
void ReportTranslateHrefHintStatus(HrefTranslateStatus status);

// Called when Chrome Translate is initiated, the navigation is from Google, and
// a href translate target is present. Records the status of any user prefs
// filtering.
void ReportTranslateHrefHintPrefsFilterStatus(
    HrefTranslatePrefsFilterStatus status);

// Called when Chrome Translate target language is determined.
void ReportTranslateTargetLanguageOrigin(TargetLanguageOrigin origin);

}  // namespace TranslateBrowserMetrics

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_BROWSER_METRICS_H_
