// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using quick_answers::prefs::ConsentStatus;
using quick_answers::prefs::kQuickAnswersConsentStatus;
using quick_answers::prefs::kQuickAnswersDefinitionEnabled;
using quick_answers::prefs::kQuickAnswersEnabled;
using quick_answers::prefs::kQuickAnswersNoticeImpressionCount;
using quick_answers::prefs::kQuickAnswersTranslationEnabled;
using quick_answers::prefs::kQuickAnswersUnitConversionEnabled;

QuickAnswersState* g_quick_answers_state = nullptr;

const char kQuickAnswersConsent[] = "QuickAnswers.V2.Consent";
const char kQuickAnswersConsentDuration[] = "QuickAnswers.V2.Consent.Duration";
const char kQuickAnswersConsentImpression[] =
    "QuickAnswers.V2.Consent.Impression";

bool IsQuickAnswersAllowedForLocale(const std::string& locale,
                                    const std::string& runtime_locale) {
  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA, ULOC_UK, ULOC_US,
                                         "en_AU",     "en_IN", "en_NZ"};
  return base::Contains(kAllowedLocales, locale) ||
         base::Contains(kAllowedLocales, runtime_locale);
}

void IncrementPrefCounter(PrefService* prefs,
                          const std::string& path,
                          int count) {
  prefs->SetInteger(path, prefs->GetInteger(path) + count);
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

// Record the consent result with how many times the user has seen the consent
// and impression duration.
void RecordConsentResult(ConsentResultType type,
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

void QuickAnswersState::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service)
    return;

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateSettingsEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersConsentStatus,
      base::BindRepeating(&QuickAnswersState::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersDefinitionEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateDefinitionEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersTranslationEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateTranslationEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersUnitConversionEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateUnitConversionEnabled,
                          base::Unretained(this)));

  UpdateSettingsEnabled();
  UpdateConsentStatus();
  UpdateDefinitionEnabled();
  UpdateTranslationEnabled();
  UpdateUnitConversionEnabled();

  prefs_initialized_ = true;

  UpdateEligibility();
}

void QuickAnswersState::StartConsent() {
  consent_start_time_ = base::TimeTicks::Now();
}

void QuickAnswersState::OnConsentResult(ConsentResultType result) {
  auto* prefs = pref_change_registrar_->prefs();

  DCHECK(!consent_start_time_.is_null());
  auto duration = base::TimeTicks::Now() - consent_start_time_;

  // Only increase the counter and record the impression if the minimum duration
  // has been reached.
  if (duration.InSeconds() >= kConsentImpressionMinimumDuration) {
    // Increments impression count.
    IncrementPrefCounter(pref_change_registrar_->prefs(),
                         kQuickAnswersNoticeImpressionCount, 1);
    RecordConsentResult(result,
                        prefs->GetInteger(kQuickAnswersNoticeImpressionCount),
                        duration);
  }

  switch (result) {
    case ConsentResultType::kAllow:
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
      // Enable Quick Answers if the user accepted the consent.
      prefs->SetBoolean(kQuickAnswersEnabled, true);
      break;
    case ConsentResultType::kNoThanks:
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
      prefs->SetBoolean(kQuickAnswersEnabled, false);
      break;
    case ConsentResultType::kDismiss:
      // If the impression count cap is reached, set the consented status to
      // false;
      bool impression_cap_reached =
          prefs->GetInteger(kQuickAnswersNoticeImpressionCount) >=
          kConsentImpressionCap;
      if (impression_cap_reached) {
        prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
        prefs->SetBoolean(kQuickAnswersEnabled, false);
      }
  }

  consent_start_time_ = base::TimeTicks();
}

bool QuickAnswersState::ShouldUseQuickAnswersTextAnnotator() {
  return base::SysInfo::IsRunningOnChromeOS() ||
         use_text_annotator_for_testing_;
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_)
    observer->OnSettingsEnabled(settings_enabled_);
}

void QuickAnswersState::UpdateSettingsEnabled() {
  auto* prefs = pref_change_registrar_->prefs();

  auto settings_enabled = prefs->GetBoolean(kQuickAnswersEnabled);
  if (settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;

  // If the user turn on the Quick Answers in settings, set the consented status
  // to true.
  if (settings_enabled_) {
    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
  }

  // If the feature is enforced off by the administrator policy, set the
  // consented status to rejected.
  if (!settings_enabled_ &&
      prefs->IsManagedPreference(quick_answers::prefs::kQuickAnswersEnabled)) {
    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
  }

  for (auto& observer : observers_)
    observer.OnSettingsEnabled(settings_enabled_);

  UpdateEligibility();
}

void QuickAnswersState::UpdateConsentStatus() {
  auto consent_status = static_cast<ConsentStatus>(
      pref_change_registrar_->prefs()->GetInteger(kQuickAnswersConsentStatus));
  if (consent_status_ == consent_status) {
    return;
  }
  consent_status_ = consent_status;
}

void QuickAnswersState::UpdateDefinitionEnabled() {
  auto definition_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersDefinitionEnabled);
  if (definition_enabled_ == definition_enabled) {
    return;
  }
  definition_enabled_ = definition_enabled;
}

void QuickAnswersState::UpdateTranslationEnabled() {
  auto translation_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersTranslationEnabled);
  if (translation_enabled_ == translation_enabled) {
    return;
  }
  translation_enabled_ = translation_enabled;
}

void QuickAnswersState::UpdateUnitConversionEnabled() {
  auto unit_conversion_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersUnitConversionEnabled);
  if (unit_conversion_enabled_ == unit_conversion_enabled) {
    return;
  }
  unit_conversion_enabled_ = unit_conversion_enabled;
}

void QuickAnswersState::UpdateEligibility() {
  if (!pref_change_registrar_)
    return;

  std::string locale = pref_change_registrar_->prefs()->GetString(
      language::prefs::kApplicationLocale);
  std::string resolved_locale;
  l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                   /*perform_io=*/false);
  is_eligible_ = IsQuickAnswersAllowedForLocale(
      resolved_locale, icu::Locale::getDefault().getName());
}
