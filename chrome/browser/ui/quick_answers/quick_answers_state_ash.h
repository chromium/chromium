// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_

#include "ash/public/cpp/session/session_observer.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

class PrefChangeRegistrar;
class PrefService;

// A class that holds Quick Answers related prefs and states.
class QuickAnswersStateAsh : public ash::SessionObserver,
                             public QuickAnswersState {
 public:
  QuickAnswersStateAsh();

  QuickAnswersStateAsh(const QuickAnswersStateAsh&) = delete;
  QuickAnswersStateAsh& operator=(const QuickAnswersStateAsh&) = delete;

  ~QuickAnswersStateAsh() override;

 private:
  // SessionObserver:
  void OnFirstSessionStarted() override;

  void RegisterPrefChanges(PrefService* pref_service);

  // QuickAnswersState:
  void StartConsent() override;
  void OnConsentResult(ConsentResultType result) override;

  // Called when the related preferences are obtained from the pref service.
  void UpdateSettingsEnabled();
  void UpdateConsentStatus();
  void UpdateDefinitionEnabled();
  void UpdateTranslationEnabled();
  void UpdateUnitConversionEnabled();
  void OnApplicationLocaleReady();
  void UpdatePreferredLanguages();
  void UpdateSpokenFeedbackEnabled();

  // Time when the notice is shown.
  base::TimeTicks consent_start_time_;

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ash::ScopedSessionObserver session_observer_;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_
