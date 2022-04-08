// Copyright 2021 The Chromium Authors. All rights reserved.
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

  prefs_initialized_ = true;

  UpdateEligibility();
}

QuickAnswersStateLacros::~QuickAnswersStateLacros() = default;

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

  auto consent_status =
      static_cast<quick_answers::prefs::ConsentStatus>(value.GetInt());
  if (consent_status_ == consent_status) {
    return;
  }
  consent_status_ = consent_status;
}

void QuickAnswersStateLacros::OnDefinitionEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  bool definition_enabled = value.GetBool();

  if (definition_enabled_ == definition_enabled) {
    return;
  }
  definition_enabled_ = definition_enabled;
}

void QuickAnswersStateLacros::OnTranslationEnabledChanged(base::Value value) {
  DCHECK(value.is_bool());
  bool translation_enabled = value.GetBool();

  if (translation_enabled_ == translation_enabled) {
    return;
  }
  translation_enabled_ = translation_enabled;
}

void QuickAnswersStateLacros::OnUnitConversionEnabledChanged(
    base::Value value) {
  DCHECK(value.is_bool());
  bool unit_conversion_enabled = value.GetBool();

  if (unit_conversion_enabled_ == unit_conversion_enabled) {
    return;
  }
  unit_conversion_enabled_ = unit_conversion_enabled;
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
  auto preferred_languages = value.GetString();

  if (preferred_languages_ == preferred_languages) {
    return;
  }
  preferred_languages_ = preferred_languages;
}
