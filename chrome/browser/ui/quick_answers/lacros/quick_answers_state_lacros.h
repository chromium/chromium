// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_LACROS_QUICK_ANSWERS_STATE_LACROS_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_LACROS_QUICK_ANSWERS_STATE_LACROS_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/lacros/crosapi_pref_observer.h"

// A class that holds Quick Answers related prefs and states in Lacros browser.
class QuickAnswersStateLacros : public QuickAnswersState {
 public:
  QuickAnswersStateLacros();

  QuickAnswersStateLacros(const QuickAnswersStateLacros&) = delete;
  QuickAnswersStateLacros& operator=(const QuickAnswersStateLacros&) = delete;

  ~QuickAnswersStateLacros() override;

 private:
  void StartConsent() override;
  void OnConsentResult(ConsentResultType result) override;

  void OnSettingsEnabledChanged(const base::Value& value);
  void OnConsentStatusChanged(const base::Value& value);
  void OnDefinitionEnabledChanged(const base::Value& value);
  void OnTranslationEnabledChanged(const base::Value& value);
  void OnUnitConversionEnabledChanged(const base::Value& value);
  void OnApplicationLocaleChanged(const base::Value& value);
  void OnPreferredLanguagesChanged(const base::Value& value);
  void OnImpressionCountChanged(const base::Value& value);
  void OnImpressionDurationChanged(const base::Value& value);

  // Time when the notice is shown.
  base::TimeTicks consent_start_time_;

  int impression_count_ = 0;
  int impression_duration_ = 0;

  std::unique_ptr<CrosapiPrefObserver> settings_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> consent_status_observer_;
  std::unique_ptr<CrosapiPrefObserver> definition_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> translation_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> unit_conversion_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> application_locale_observer_;
  std::unique_ptr<CrosapiPrefObserver> preferred_languages_observer_;
  std::unique_ptr<CrosapiPrefObserver> impression_count_observer_;
  std::unique_ptr<CrosapiPrefObserver> impression_duration_observer_;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_LACROS_QUICK_ANSWERS_STATE_LACROS_H_
