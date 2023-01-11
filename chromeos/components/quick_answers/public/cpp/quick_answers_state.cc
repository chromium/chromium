// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

const char kQuickAnswersConsent[] = "QuickAnswers.V2.Consent";
const char kQuickAnswersConsentDuration[] = "QuickAnswers.V2.Consent.Duration";
const char kQuickAnswersConsentImpression[] =
    "QuickAnswers.V2.Consent.Impression";

// Supported languages of the Quick Answers feature.
const std::string kSupportedLanguages[] = {"en", "es", "it", "fr", "pt", "de"};

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

bool QuickAnswersState::IsSupportedLanguage(const std::string& language) {
  return base::Contains(kSupportedLanguages, language);
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_) {
    observer->OnPrefsInitialized();
    observer->OnSettingsEnabled(settings_enabled_);
    observer->OnConsentStatusUpdated(consent_status_);
    observer->OnApplicationLocaleReady(resolved_application_locale_);
    observer->OnPreferredLanguagesChanged(preferred_languages_);
    observer->OnEligibilityChanged(is_eligible_);
  }
}

void QuickAnswersState::UpdateEligibility() {
  if (resolved_application_locale_.empty())
    return;

  bool is_eligible =
      IsSupportedLanguage(l10n_util::GetLanguage(resolved_application_locale_));

  if (is_eligible_ == is_eligible)
    return;
  is_eligible_ = is_eligible;

  for (auto& observer : observers_) {
    observer.OnEligibilityChanged(is_eligible_);
  }
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
