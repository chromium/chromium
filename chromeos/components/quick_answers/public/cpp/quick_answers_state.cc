// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

const char kQuickAnswersConsent[] = "QuickAnswers.V2.Consent";
const char kQuickAnswersConsentDuration[] = "QuickAnswers.V2.Consent.Duration";
const char kQuickAnswersConsentImpression[] =
    "QuickAnswers.V2.Consent.Impression";

bool IsQuickAnswersAllowedForLocale(const std::string& locale,
                                    const std::string& runtime_locale) {
  if (chromeos::features::IsQuickAnswersForMoreLocalesEnabled())
    return true;

  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA, ULOC_UK, ULOC_US,
                                         "en_AU",     "en_IN", "en_NZ"};
  return base::Contains(kAllowedLocales, locale) ||
         base::Contains(kAllowedLocales, runtime_locale);
}

std::string ConsentResultTypeToString(ConsentResultType type) {
  switch (type) {
    case ConsentResultType::kAllow:
      return "Allow";
    case ConsentResultType::kNoThanks:
      return "NoThanks";
    case ConsentResultType::kDismiss:
      return "Dismiss";
  }
}

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

QuickAnswersState::QuickAnswersState() {
  DCHECK(!g_quick_answers_state);
  g_quick_answers_state = this;
}

QuickAnswersState::~QuickAnswersState() {
  DCHECK_EQ(g_quick_answers_state, this);
  g_quick_answers_state = nullptr;
}

void QuickAnswersState::AddObserver(QuickAnswersStateObserver* observer) {
  observers_.AddObserver(observer);
  InitializeObserver(observer);
}

void QuickAnswersState::RemoveObserver(QuickAnswersStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool QuickAnswersState::ShouldUseQuickAnswersTextAnnotator() {
  return base::SysInfo::IsRunningOnChromeOS() ||
         use_text_annotator_for_testing_;
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_) {
    observer->OnSettingsEnabled(settings_enabled_);
    observer->OnApplicationLocaleReady(resolved_application_locale_);
  }
}

void QuickAnswersState::UpdateEligibility() {
  if (resolved_application_locale_.empty())
    return;

  is_eligible_ = IsQuickAnswersAllowedForLocale(
      resolved_application_locale_, icu::Locale::getDefault().getName());
}

void QuickAnswersState::RecordConsentResult(ConsentResultType type,
                                            int nth_impression,
                                            const base::TimeDelta duration) {
  base::UmaHistogramExactLinear(kQuickAnswersConsent, nth_impression,
                                kConsentImpressionCap);

  std::string interaction_type = ConsentResultTypeToString(type);
  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.%s", kQuickAnswersConsentImpression,
                         interaction_type.c_str()),
      nth_impression, kConsentImpressionCap);
  base::UmaHistogramTimes(
      base::StringPrintf("%s.%s", kQuickAnswersConsentDuration,
                         interaction_type.c_str()),
      duration);
}
