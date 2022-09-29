// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"

FakeQuickAnswersState::FakeQuickAnswersState() = default;

FakeQuickAnswersState::~FakeQuickAnswersState() = default;

void FakeQuickAnswersState::SetSettingsEnabled(bool settings_enabled) {
  if (settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;

  for (auto& observer : observers_) {
    observer.OnSettingsEnabled(settings_enabled_);
  }
}

void FakeQuickAnswersState::SetConsentStatus(
    quick_answers::prefs::ConsentStatus consent_status) {
  if (consent_status_ == consent_status) {
    return;
  }
  consent_status_ = consent_status;

  for (auto& observer : observers_) {
    observer.OnConsentStatusUpdated(consent_status_);
  }
}

void FakeQuickAnswersState::SetApplicationLocale(const std::string& locale) {
  if (resolved_application_locale_ == locale) {
    return;
  }
  resolved_application_locale_ = locale;

  for (auto& observer : observers_) {
    observer.OnApplicationLocaleReady(locale);
  }

  UpdateEligibility();
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

  UpdateEligibility();
}
