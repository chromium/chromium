// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/lacros/quick_answers_state_lacros.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using quick_answers::prefs::ConsentStatus;

void SetPref(crosapi::mojom::PrefPath path, base::Value value) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, std::move(value), base::DoNothing());
}

}  // namespace

QuickAnswersStateLacros::QuickAnswersStateLacros() {
  // The observers are fired immediate with the current pref value on
  // initialization.
  settings_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnSettingsEnabledChanged,
                          base::Unretained(this)));
  consent_status_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
      base::BindRepeating(&QuickAnswersStateLacros::OnConsentStatusChanged,
                          base::Unretained(this)));
  definition_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersDefinitionEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnDefinitionEnabledChanged,
                          base::Unretained(this)));
  translation_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersTranslationEnabled,
      base::BindRepeating(&QuickAnswersStateLacros::OnTranslationEnabledChanged,
                          base::Unretained(this)));
  unit_conversion_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersUnitConversionEnabled,
      base::BindRepeating(
          &QuickAnswersStateLacros::OnUnitConversionEnabledChanged,
          base::Unretained(this)));
  application_locale_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kApplicationLocale,
      base::BindRepeating(&QuickAnswersStateLacros::OnApplicationLocaleChanged,
                          base::Unretained(this)));
  preferred_languages_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kPreferredLanguages,
      base::BindRepeating(&QuickAnswersStateLacros::OnPreferredLanguagesChanged,
                          base::Unretained(this)));
  spoken_feedback_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(
          &QuickAnswersStateLacros::OnSpokenFeedbackEnabledChanged,
          base::Unretained(this)));
  impression_count_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionCount,
      base::BindRepeating(&QuickAnswersStateLacros::OnImpressionCountChanged,
                          base::Unretained(this)));
  impression_duration_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionDuration,
      base::BindRepeating(&QuickAnswersStateLacros::OnImpressionDurationChanged,
                          base::Unretained(this)));

  prefs_initialized_ = true;
  for (auto& observer : observers_)
    observer.OnPrefsInitialized();

  UpdateEligibility();
}

QuickAnswersStateLacros::~QuickAnswersStateLacros() = default;

void QuickAnswersStateLacros::StartConsent() {
  consent_start_time_ = base::TimeTicks::Now();
}

void QuickAnswersStateLacros::OnConsentResult(ConsentResultType result) {
  DCHECK(!consent_start_time_.is_null());
  auto duration = base::TimeTicks::Now() - consent_start_time_;

  auto new_impression_count = impression_count_;

  // Only increase the counter and record the impression if the minimum duration
  // has been reached.
  if (duration.InSeconds() >= kConsentImpressionMinimumDuration) {
    ++new_impression_count;
    // Increments impression count.
    SetPref(crosapi::mojom::PrefPath::kQuickAnswersNoticeImpressionCount,
            base::Value(new_impression_count));
    RecordConsentResult(result, new_impression_count, duration);
  }

  switch (result) {
    case ConsentResultType::kAllow:
      SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
              base::Value(ConsentStatus::kAccepted));
      // Enable Quick Answers if the user accepted the consent.
      SetPref(crosapi::mojom::PrefPath::kQuickAnswersEnabled,
              base::Value(true));
      break;
    case ConsentResultType::kNoThanks:
      SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
              base::Value(ConsentStatus::kRejected));
      SetPref(crosapi::mojom::PrefPath::kQuickAnswersEnabled,
              base::Value(false));
      break;
    case ConsentResultType::kDismiss:
      // If the impression count cap is reached, set the consented status to
      // false;
      bool impression_cap_reached =
          new_impression_count >= kConsentImpressionCap;
      if (impression_cap_reached) {
        SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
                base::Value(ConsentStatus::kRejected));
        SetPref(crosapi::mojom::PrefPath::kQuickAnswersEnabled,
                base::Value(false));
      }
  }

  consent_start_time_ = base::TimeTicks();
}

void QuickAnswersStateLacros::OnSettingsEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  bool settings_enabled = value.GetBool();

  if (settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;

  // If the user turn on the Quick Answers in settings, set the consented status
  // to true.
  if (settings_enabled_) {
    SetPref(crosapi::mojom::PrefPath::kQuickAnswersConsentStatus,
            base::Value(quick_answers::prefs::ConsentStatus::kAccepted));
  }

  for (auto& observer : observers_)
    observer.OnSettingsEnabled(settings_enabled_);
}

void QuickAnswersStateLacros::OnConsentStatusChanged(base::Value value) {
  DCHECK(value.is_int());
  consent_status_ =
      static_cast<quick_answers::prefs::ConsentStatus>(value.GetInt());

  for (auto& observer : observers_)
    observer.OnConsentStatusUpdated(consent_status_);
}

void QuickAnswersStateLacros::OnDefinitionEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  definition_enabled_ = value.GetBool();
}

void QuickAnswersStateLacros::OnTranslationEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  translation_enabled_ = value.GetBool();
}

void QuickAnswersStateLacros::OnUnitConversionEnabledChanged(
    base::Value value) {
  DCHECK(value.is_bool());
  unit_conversion_enabled_ = value.GetBool();
}

void QuickAnswersStateLacros::OnApplicationLocaleChanged(base::Value value) {
  DCHECK(value.is_string());
  auto locale = value.GetString();

  if (locale.empty())
    return;

  // We should not directly use the pref locale, resolve the generic locale name
  // to one of the locally defined ones first.
  std::string resolved_locale;
  bool resolve_success =
      l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                       /*perform_io=*/false);
  DCHECK(resolve_success);

  if (resolved_application_locale_ == resolved_locale) {
    return;
  }
  resolved_application_locale_ = resolved_locale;

  for (auto& observer : observers_) {
    observer.OnApplicationLocaleReady(resolved_locale);
  }

  UpdateEligibility();
}

void QuickAnswersStateLacros::OnPreferredLanguagesChanged(base::Value value) {
  DCHECK(value.is_string());
  preferred_languages_ = value.GetString();

  for (auto& observer : observers_)
    observer.OnPreferredLanguagesChanged(preferred_languages_);
}

void QuickAnswersStateLacros::OnImpressionCountChanged(base::Value value) {
  DCHECK(value.is_int());
  impression_count_ = value.GetInt();
}

void QuickAnswersStateLacros::OnImpressionDurationChanged(base::Value value) {
  DCHECK(value.is_int());
  impression_duration_ = value.GetInt();
}

void QuickAnswersStateLacros::OnSpokenFeedbackEnabledChanged(
    base::Value value) {
  DCHECK(value.is_bool());
  spoken_feedback_enabled_ = value.GetBool();
}
