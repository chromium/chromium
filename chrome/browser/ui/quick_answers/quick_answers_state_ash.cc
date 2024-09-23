// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
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
  pref_change_registrar_->Add(
      kQuickAnswersNoticeImpressionCount,
      base::BindRepeating(&QuickAnswersStateAsh::UpdateNoticeImpressionCount,
                          base::Unretained(this)));

  UpdateSettingsEnabled();
  UpdateConsentStatus();
  UpdateDefinitionEnabled();
  UpdateTranslationEnabled();
  UpdateUnitConversionEnabled();
  OnApplicationLocaleReady();
  UpdatePreferredLanguages();
  UpdateSpokenFeedbackEnabled();
  UpdateNoticeImpressionCount();

  prefs_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnPrefsInitialized();
  }

  quick_answers::RecordFeatureEnabled(IsEnabledAs(FeatureType::kQuickAnswers));

  MaybeNotifyEligibilityChanged();
  MaybeNotifyIsEnabledChanged();
}

void QuickAnswersStateAsh::AsyncWriteConsentUiImpressionCount(int32_t count) {
  pref_change_registrar_->prefs()->SetInteger(
      kQuickAnswersNoticeImpressionCount, count);
}

void QuickAnswersStateAsh::AsyncWriteConsentStatus(
    ConsentStatus consent_status) {
  pref_change_registrar_->prefs()->SetInteger(kQuickAnswersConsentStatus,
                                              consent_status);
}

void QuickAnswersStateAsh::AsyncWriteEnabled(bool enabled) {
  pref_change_registrar_->prefs()->SetBoolean(kQuickAnswersEnabled, enabled);
}

void QuickAnswersStateAsh::UpdateSettingsEnabled() {
  auto* prefs = pref_change_registrar_->prefs();

  // TODO(b/340628526): modifying a state is error-prone. For example, if a
  // state is read before a modification happens, a stale state will be read.
  // Instead, each state (e.g., IsEnabled) should be calculated from other
  // dependent states (e.g., IsKioskSession).
  bool settings_enabled = prefs->GetBoolean(kQuickAnswersEnabled);

  // Quick answers should be disabled for kiosk session.
  if (chromeos::IsKioskSession()) {
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

  bool turned_on = quick_answers_enabled_.has_value() &&
                   !quick_answers_enabled_.value() && settings_enabled;

  // If the user turn on the Quick Answers in settings, set the consented status
  // to true.
  // TODO(b/340628526): move this logic to `QuickAnswersState`.
  // `QuickAnswersStateAsh` should only have logic unique to ash. Plan is to
  // make `quick_answers_enabled_` as private and add void
  // SetQuickAnswersEnabled(bool) as a protected method.
  if (turned_on) {
    CHECK(quick_answers_enabled_.has_value());
    CHECK(!quick_answers_enabled_.value());
    CHECK(settings_enabled);

    prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
  }

  quick_answers_enabled_ = settings_enabled;
  MaybeNotifyIsEnabledChanged();
}

void QuickAnswersStateAsh::UpdateConsentStatus() {
  auto consent_status = static_cast<ConsentStatus>(
      pref_change_registrar_->prefs()->GetInteger(kQuickAnswersConsentStatus));

  SetQuickAnswersFeatureConsentStatus(consent_status);
}

void QuickAnswersStateAsh::UpdateDefinitionEnabled() {
  auto definition_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersDefinitionEnabled);

  // See a comment of `QuickAnswersState::IsIntentEligible` for the reason of
  // this is called as eligible instead of enabled.
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kDefinition,
                                     definition_enabled);
}

void QuickAnswersStateAsh::UpdateTranslationEnabled() {
  auto translation_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersTranslationEnabled);

  // See a comment of `QuickAnswersState::IsIntentEligible` for the reason of
  // this is called as eligible instead of enabled.
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kTranslation,
                                     translation_enabled);
}

void QuickAnswersStateAsh::UpdateUnitConversionEnabled() {
  auto unit_conversion_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersUnitConversionEnabled);

  // See a comment of `QuickAnswersState::IsIntentEligible` for the reason of
  // this is called as eligible instead of enabled.
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kUnitConversion,
                                     unit_conversion_enabled);
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

  MaybeNotifyEligibilityChanged();
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

void QuickAnswersStateAsh::UpdateNoticeImpressionCount() {
  consent_ui_impression_count_ = pref_change_registrar_->prefs()->GetInteger(
      kQuickAnswersNoticeImpressionCount);
}
