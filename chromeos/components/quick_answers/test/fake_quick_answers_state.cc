// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"

#include "base/observer_list.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"

FakeQuickAnswersState::FakeQuickAnswersState() = default;

FakeQuickAnswersState::~FakeQuickAnswersState() = default;

void FakeQuickAnswersState::SetSettingsEnabled(bool settings_enabled) {
  AsyncWriteEnabled(settings_enabled);
}

void FakeQuickAnswersState::SetApplicationLocale(const std::string& locale) {
  if (resolved_application_locale_ == locale) {
    return;
  }
  resolved_application_locale_ = locale;

  for (auto& observer : observers_) {
    observer.OnApplicationLocaleReady(locale);
  }

  MaybeNotifyEligibilityChanged();
}

void FakeQuickAnswersState::SetPreferredLanguages(
    const std::string& preferred_languages) {
  if (preferred_languages_ == preferred_languages) {
    return;
  }
  preferred_languages_ = preferred_languages;

  for (auto& observer : observers_) {
    observer.OnPreferredLanguagesChanged(preferred_languages);
  }
}

void FakeQuickAnswersState::OnPrefsInitialized() {
  prefs_initialized_ = true;

  for (auto& observer : observers_) {
    observer.OnPrefsInitialized();
  }

  MaybeNotifyEligibilityChanged();
  MaybeNotifyIsEnabledChanged();
}

void FakeQuickAnswersState::SetDefinitionEligible(bool eligible) {
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kDefinition,
                                     eligible);
}

void FakeQuickAnswersState::SetTranslationEligible(bool eligible) {
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kTranslation,
                                     eligible);
}

void FakeQuickAnswersState::SetUnitConversionEligible(bool eligible) {
  SetIntentEligibilityAsQuickAnswers(quick_answers::Intent::kUnitConversion,
                                     eligible);
}

void FakeQuickAnswersState::OverrideFeatureType(
    QuickAnswersState::FeatureType feature_type) {
  feature_type_ = feature_type;
}

void FakeQuickAnswersState::AsyncWriteConsentUiImpressionCount(int32_t count) {
  consent_ui_impression_count_ = count;
}

void FakeQuickAnswersState::AsyncWriteConsentStatus(
    quick_answers::prefs::ConsentStatus consent_status) {
  SetQuickAnswersFeatureConsentStatus(consent_status);
}

void FakeQuickAnswersState::AsyncWriteEnabled(bool enabled) {
  quick_answers_enabled_ = enabled;

  MaybeNotifyIsEnabledChanged();
}

base::expected<QuickAnswersState::FeatureType, QuickAnswersState::Error>
FakeQuickAnswersState::GetFeatureTypeExpected() const {
  if (feature_type_) {
    return feature_type_.value();
  }

  return QuickAnswersState::GetFeatureTypeExpected();
}
