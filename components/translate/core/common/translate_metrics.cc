// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_metrics.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "url/url_constants.h"

namespace translate {

namespace metrics_internal {

const char kTranslateContentLanguage[] = "Translate.ContentLanguage";
const char kTranslateHtmlLang[] = "Translate.HtmlLang";
const char kTranslateLanguageVerification[] = "Translate.LanguageVerification";
const char kTranslateTimeToBeReady[] = "Translate.Translation.TimeToBeReady";
const char kTranslateTimeToLoad[] = "Translate.Translation.TimeToLoad";
const char kTranslateTimeToTranslate[] =
    "Translate.Translation.TimeToTranslate";
const char kTranslateUserActionDuration[] = "Translate.UserActionDuration";
const char kTranslatePageScheme[] = "Translate.PageScheme";
const char kTranslateSimilarLanguageMatch[] = "Translate.SimilarLanguageMatch";
const char kTranslateLanguageDeterminedDuration[] =
    "Translate.LanguageDeterminedDuration";

}  // namespace metrics_internal

namespace {

LanguageCheckType GetLanguageCheckMetric(const std::string& provided_code,
                                         const std::string& revised_code) {
  if (provided_code.empty())
    return LANGUAGE_NOT_PROVIDED;
  else if (provided_code == revised_code)
    return LANGUAGE_VALID;
  return LANGUAGE_INVALID;
}

}  // namespace

void ReportContentLanguage(const std::string& provided_code,
                           const std::string& revised_code) {
  UMA_HISTOGRAM_ENUMERATION(metrics_internal::kTranslateContentLanguage,
                            GetLanguageCheckMetric(provided_code, revised_code),
                            LANGUAGE_MAX);
}

void ReportHtmlLang(const std::string& provided_code,
                    const std::string& revised_code) {
  UMA_HISTOGRAM_ENUMERATION(metrics_internal::kTranslateHtmlLang,
                            GetLanguageCheckMetric(provided_code, revised_code),
                            LANGUAGE_MAX);
}

void ReportLanguageVerification(LanguageVerificationType type) {
  UMA_HISTOGRAM_ENUMERATION(metrics_internal::kTranslateLanguageVerification,
                            type, LANGUAGE_VERIFICATION_MAX);
}

void ReportTimeToBeReady(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(metrics_internal::kTranslateTimeToBeReady,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToLoad(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(metrics_internal::kTranslateTimeToLoad,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToTranslate(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(metrics_internal::kTranslateTimeToTranslate,
                             base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportUserActionDuration(base::TimeTicks begin, base::TimeTicks end) {
  UMA_HISTOGRAM_LONG_TIMES(metrics_internal::kTranslateUserActionDuration,
                           end - begin);
}

void ReportPageScheme(const std::string& scheme) {
  SchemeType type = SCHEME_OTHERS;
  if (scheme == url::kHttpScheme)
    type = SCHEME_HTTP;
  else if (scheme == url::kHttpsScheme)
    type = SCHEME_HTTPS;
  UMA_HISTOGRAM_ENUMERATION(metrics_internal::kTranslatePageScheme, type,
                            SCHEME_MAX);
}

void ReportSimilarLanguageMatch(bool match) {
  UMA_HISTOGRAM_BOOLEAN(metrics_internal::kTranslateSimilarLanguageMatch,
                        match);
}

void ReportLanguageDeterminedDuration(base::TimeTicks begin,
                                      base::TimeTicks end) {
  UMA_HISTOGRAM_LONG_TIMES(
      metrics_internal::kTranslateLanguageDeterminedDuration, end - begin);
}

}  // namespace translate
