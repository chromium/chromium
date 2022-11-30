// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"

// The consent will appear up to a total of 6 times.
constexpr int kConsentImpressionCap = 6;
// The consent need to show for at least 1 second to be counted.
constexpr int kConsentImpressionMinimumDuration = 1;

// Consent result of the consent-view.
enum class ConsentResultType {
  // When user clicks on the "Allow" button.
  kAllow = 0,
  // When user clicks on the "No thanks" button.
  kNoThanks = 1,
  // When user dismisses or ignores the consent-view.
  kDismiss = 2
};

// A checked observer which receives Quick Answers state change.
class QuickAnswersStateObserver : public base::CheckedObserver {
 public:
  virtual void OnSettingsEnabled(bool enabled) {}
  virtual void OnConsentStatusUpdated(
      quick_answers::prefs::ConsentStatus status) {}
  virtual void OnApplicationLocaleReady(const std::string& locale) {}
  virtual void OnPreferredLanguagesChanged(
      const std::string& preferred_languages) {}
  virtual void OnEligibilityChanged(bool eligible) {}
  virtual void OnPrefsInitialized() {}
};

// A class that holds Quick Answers related prefs and states.
class QuickAnswersState {
 public:
  static QuickAnswersState* Get();

  QuickAnswersState();

  QuickAnswersState(const QuickAnswersState&) = delete;
  QuickAnswersState& operator=(const QuickAnswersState&) = delete;

  virtual ~QuickAnswersState();

  void AddObserver(QuickAnswersStateObserver* observer);
  void RemoveObserver(QuickAnswersStateObserver* observer);

  virtual void StartConsent() {}
  virtual void OnConsentResult(ConsentResultType result) {}

  bool ShouldUseQuickAnswersTextAnnotator();

  bool IsSupportedLanguage(const std::string& language);

  bool settings_enabled() const { return settings_enabled_; }
  quick_answers::prefs::ConsentStatus consent_status() const {
    return consent_status_;
  }
  bool definition_enabled() const { return definition_enabled_; }
  bool translation_enabled() const { return translation_enabled_; }
  bool unit_conversion_enabled() const { return unit_conversion_enabled_; }
  const std::string& application_locale() const {
    return resolved_application_locale_;
  }
  const std::string& preferred_languages() const {
    return preferred_languages_;
  }
  bool spoken_feedback_enabled() const { return spoken_feedback_enabled_; }
  bool is_eligible() const { return is_eligible_; }
  bool prefs_initialized() const { return prefs_initialized_; }

  void set_eligibility_for_testing(bool is_eligible) {
    is_eligible_ = is_eligible;
  }
  void set_use_text_annotator_for_testing() {
    use_text_annotator_for_testing_ = true;
  }

 protected:
  void InitializeObserver(QuickAnswersStateObserver* observer);

  // Called when the feature eligibility might change.
  void UpdateEligibility();

  // Record the consent result with how many times the user has seen the consent
  // and impression duration.
  void RecordConsentResult(ConsentResultType type,
                           int nth_impression,
                           const base::TimeDelta duration);

  // Whether the Quick Answers is enabled in system settings.
  bool settings_enabled_ = false;

  // Status of the user's consent for the Quick Answers feature.
  quick_answers::prefs::ConsentStatus consent_status_ =
      quick_answers::prefs::ConsentStatus::kUnknown;

  // Whether the Quick Answers definition is enabled.
  bool definition_enabled_ = true;

  // Whether the Quick Answers translation is enabled.
  bool translation_enabled_ = true;

  // Whether the Quick Answers unit conversion is enabled.
  bool unit_conversion_enabled_ = true;

  // The resolved application locale.
  std::string resolved_application_locale_;

  // The list of preferred languages, separated by comma.
  // (ex. "en-US,zh,fr").
  std::string preferred_languages_;

  // Whether the a11y spoken feedback tool is enabled.
  bool spoken_feedback_enabled_;

  // Whether the Quick Answers feature is eligible. The value is derived from a
  // number of other states.
  bool is_eligible_ = false;

  // Whether the pref values has been initialized.
  bool prefs_initialized_ = false;

  // Whether to use text annotator for testing.
  bool use_text_annotator_for_testing_ = false;

  base::ObserverList<QuickAnswersStateObserver> observers_;
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_
