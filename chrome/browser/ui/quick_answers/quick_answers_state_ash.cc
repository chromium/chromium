// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using quick_answers::prefs::ConsentStatus;
using quick_answers::prefs::kQuickAnswersConsentStatus;
using quick_answers::prefs::kQuickAnswersDefinitionEnabled;
using quick_answers::prefs::kQuickAnswersEnabled;
using quick_answers::prefs::kQuickAnswersNoticeImpressionCount;
using quick_answers::prefs::kQuickAnswersTranslationEnabled;
using quick_answers::prefs::kQuickAnswersUnitConversionEnabled;

void IncrementPrefCounter(PrefService* prefs,
                          const std::string& path,
                          int count) {
  prefs->SetInteger(path, prefs->GetInteger(path) + count);
}

}  // namespace

QuickAnswersStateAsh::QuickAnswersStateAsh() {
  shell_observation_.Observe(ash::Shell::Get());

  auto* session_controller = ash::Shell::Get()->session_controller();
  CHECK(session_controller);

  session_observation_.Observe(session_controller);

  // Register pref changes if use session already started.
  if (session_controller->IsActiveUserSessionStarted()) {
    PrefService* prefs = session_controller->GetPrimaryUserPrefService();
    DCHECK(prefs);
    RegisterPrefChanges(prefs);
  }
}

QuickAnswersStateAsh::~QuickAnswersStateAsh() = default;

void QuickAnswersStateAsh::OnFirstSessionStarted() {
  PrefService* prefs =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  RegisterPrefChanges(prefs);
}

void QuickAnswersStateAsh::OnChromeTerminating() {
  session_observation_.Reset();
}

void QuickAnswersStateAsh::OnShellDestroying() {
  session_observation_.Reset();
  shell_observation_.Reset();
}

void QuickAnswersStateAsh::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service) {
    return;
  }

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateSettingsEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersConsentStatus,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersDefinitionEnabled,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateDefinitionEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersTranslationEnabled,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateTranslationEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersUnitConversionEnabled,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateUnitConversionEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      language::prefs::kApplicationLocale,
      base::BindRepeating(&QuickAnswersStateAsh::OnApplicationLocaleReady,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      language::prefs::kPreferredLanguages,
      base::BindRepeating(&QuickAnswersStateAsh::UpdatePreferredLanguages,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateSpokenFeedbackEnabled,
                          base::Unretained(this)));

  UpdateSettingsEnabled();
  UpdateConsentStatus();
  UpdateDefinitionEnabled();
  UpdateTranslationEnabled();
  UpdateUnitConversionEnabled();
  OnApplicationLocaleReady();
  UpdatePreferredLanguages();
  UpdateSpokenFeedbackEnabled();

  prefs_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnPrefsInitialized();
  }

  quick_answers::RecordFeatureEnabled(
      pref_service->GetBoolean(kQuickAnswersEnabled));

  UpdateEligibility();
}

void QuickAnswersStateAsh::StartConsent() {
  consent_start_time_ = base::TimeTicks::Now();
}

void QuickAnswersStateAsh::OnConsentResult(ConsentResultType result) {
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
      // Enable Quick Answers if the user accepted the consent.
      prefs->SetBoolean(kQuickAnswersEnabled, true);
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
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

void QuickAnswersStateAsh::UpdateSettingsEnabled() {
  auto* prefs = pref_change_registrar_->prefs();

  auto settings_enabled = prefs->GetBoolean(kQuickAnswersEnabled);

  // Quick answers should be disabled for kiosk session.
  if (chromeos::IsKioskSession() && settings_enabled) {
    settings_enabled = false;
    prefs->SetBoolean(kQuickAnswersEnabled, false);
    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
  }

  // If the feature is enforced off by the administrator policy, set the
  // consented status to rejected. This must be put before the same value return
  // below as the default value is `false` and we cannot observe
  // unmanaged-disabled to managed-disabled change.
  if (!settings_enabled &&
      prefs->IsManagedPreference(quick_answers::prefs::kQuickAnswersEnabled)) {
    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
  }

  if (settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;

  // If the user turn on the Quick Answers in settings, set the consented status
  // to true.
  if (settings_enabled_) {
    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
  }

  for (auto& observer : observers_) {
    observer.OnSettingsEnabled(settings_enabled_);
  }
}

void QuickAnswersStateAsh::UpdateConsentStatus() {
  auto consent_status = static_cast<ConsentStatus>(
      pref_change_registrar_->prefs()->GetInteger(kQuickAnswersConsentStatus));

  consent_status_ = consent_status;

  for (auto& observer : observers_) {
    observer.OnConsentStatusUpdated(consent_status_);
  }
}

void QuickAnswersStateAsh::UpdateDefinitionEnabled() {
  auto definition_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersDefinitionEnabled);

  definition_enabled_ = definition_enabled;
}

void QuickAnswersStateAsh::UpdateTranslationEnabled() {
  auto translation_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersTranslationEnabled);

  translation_enabled_ = translation_enabled;
}

void QuickAnswersStateAsh::UpdateUnitConversionEnabled() {
  auto unit_conversion_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersUnitConversionEnabled);

  unit_conversion_enabled_ = unit_conversion_enabled;
}

void QuickAnswersStateAsh::OnApplicationLocaleReady() {
  auto locale = pref_change_registrar_->prefs()->GetString(
      language::prefs::kApplicationLocale);

  if (locale.empty()) {
    return;
  }

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

void QuickAnswersStateAsh::UpdatePreferredLanguages() {
  auto preferred_languages = pref_change_registrar_->prefs()->GetString(
      language::prefs::kPreferredLanguages);

  preferred_languages_ = preferred_languages;

  for (auto& observer : observers_) {
    observer.OnPreferredLanguagesChanged(preferred_languages);
  }
}

void QuickAnswersStateAsh::UpdateSpokenFeedbackEnabled() {
  auto spoken_feedback_enabled = pref_change_registrar_->prefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  spoken_feedback_enabled_ = spoken_feedback_enabled;
}
