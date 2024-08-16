// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {
class SessionController;
class Shell;
}  // namespace ash

// TODO(b/340628526): Put this under quick_answers namespace.

// A class that holds Quick Answers related prefs and states.
class QuickAnswersStateAsh : public ash::SessionObserver,
                             public ash::ShellObserver,
                             public QuickAnswersState {
 public:
  QuickAnswersStateAsh();

  QuickAnswersStateAsh(const QuickAnswersStateAsh&) = delete;
  QuickAnswersStateAsh& operator=(const QuickAnswersStateAsh&) = delete;

  ~QuickAnswersStateAsh() override;

 private:
  // ash::SessionObserver:
  void OnFirstSessionStarted() override;
  void OnChromeTerminating() override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  void RegisterPrefChanges(PrefService* pref_service);

  // QuickAnswersState:
  void AsyncWriteConsentUiImpressionCount(int32_t count) override;
  void AsyncWriteConsentStatus(
      quick_answers::prefs::ConsentStatus consent_status) override;
  void AsyncWriteEnabled(bool enabled) override;

  // Called when the related preferences are obtained from the pref service.
  void UpdateSettingsEnabled();
  void UpdateConsentStatus();
  void UpdateDefinitionEnabled();
  void UpdateTranslationEnabled();
  void UpdateUnitConversionEnabled();
  void OnApplicationLocaleReady();
  void UpdatePreferredLanguages();
  void UpdateSpokenFeedbackEnabled();
  void UpdateNoticeImpressionCount();

  // Time when the notice is shown.
  base::TimeTicks consent_start_time_;

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<ash::SessionController, ash::SessionObserver>
      session_observation_{this};

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_STATE_ASH_H_
