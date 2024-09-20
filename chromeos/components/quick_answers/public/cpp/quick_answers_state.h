// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"

// TODO(b/340628526): Put this under quick_answers namespace.

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

  // TODO(b/340628526): Delete this method. Each value observer is called once a
  // pref gets initialized.
  virtual void OnPrefsInitialized() {}
};

// `QuickAnswersState` manages states related to Quick Answers as a capability.
// `QuickAnswersState` allows you to query states of Quick Answers capability in
// a specified feature context, e.g., check if Quick Answers capability is
// available under Hmr feature. `QuickAnswersState` expect that `FeatureType`
// does not change in a session.
//
// Terminology:
// - Quick Answers capability: a capability where a user can find definition,
//   translation, unit conversion of a right-clicked text.
// - Quick Answers feature: a feature called Quick Answers. It provides Quick
//   Answers capabpility.
// - Hmr feature: a feature called Hmr. It provides Mahi and Quick Answers
//   capability.
class QuickAnswersState : chromeos::MagicBoostState::Observer {
 public:
  enum class FeatureType {
    kHmr,
    kQuickAnswers,
  };

  enum class Error {
    kUninitialized,
  };

  static QuickAnswersState* Get();

  static FeatureType GetFeatureType();

  // Accessor methods. Those methods handle error cases (Error::kUninitialized,
  // etc) in a fail-safe way.
  static bool IsEligible();
  static bool IsEligibleAs(FeatureType feature_type);
  static bool IsEnabled();
  static bool IsEnabledAs(FeatureType feature_type);
  // `GetConsentStatus` returns `base::expected` instead of falling back to a
  // fail-safe value. `kUnknown` is not a desired fallback value for some cases.
  static base::expected<quick_answers::prefs::ConsentStatus,
                        QuickAnswersState::Error>
  GetConsentStatus();
  static base::expected<quick_answers::prefs::ConsentStatus,
                        QuickAnswersState::Error>
  GetConsentStatusAs(FeatureType feature_type);
  // Intent generation can be done before a feature is enabled to show a user
  // consent UI. Use a word eligible instead of enabled to make it clear that
  // it's not gated by `IsEnabled`.
  static bool IsIntentEligible(quick_answers::Intent intent);
  static bool IsIntentEligibleAs(quick_answers::Intent intent,
                                 FeatureType feature_type);

  QuickAnswersState();

  QuickAnswersState(const QuickAnswersState&) = delete;
  QuickAnswersState& operator=(const QuickAnswersState&) = delete;

  ~QuickAnswersState() override;

  // Observers are notified only in the context of current feature type.
  void AddObserver(QuickAnswersStateObserver* observer);
  void RemoveObserver(QuickAnswersStateObserver* observer);

  // chromeos::MagicBoostState::Observer:
  void OnHMREnabledUpdated(bool enabled) override;
  void OnHMRConsentStatusUpdated(
      chromeos::HMRConsentStatus consent_status) override;
  void OnIsDeleting() override;

  // Write consent status and a respective enabled state to the pref. Note that
  // this method returns BEFORE a write is completed. Reading consent status
  // and/or enabled state immediately after the write can read a stale value.
  // TODO(b/340628526): Add validations, e.g., fail to set kAccepted if it's in
  // kiosk mode, etc.
  void AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus consent_status);

  // Increment impression count and returns an incremented count. Note that this
  // method is not thread safe, i.e., this does NOT operate an increment as an
  // atomic operation. Reading impression count immediately after the write can
  // read a stale value.
  int32_t AsyncIncrementImpressionCount();

  bool ShouldUseQuickAnswersTextAnnotator();

  bool IsSupportedLanguage(const std::string& language) const;

  const std::string& application_locale() const {
    return resolved_application_locale_;
  }
  const std::string& preferred_languages() const {
    return preferred_languages_;
  }
  bool spoken_feedback_enabled() const { return spoken_feedback_enabled_; }
  bool prefs_initialized() const { return prefs_initialized_; }

  void SetEligibilityForTesting(bool is_eligible);
  void set_use_text_annotator_for_testing() {
    use_text_annotator_for_testing_ = true;
  }

 protected:
  // All AsyncWrite.+ functions return BEFORE a write is completed, i.e., write
  // can be an async operation. Immediately reading a respective value might end
  // up a stale value.
  virtual void AsyncWriteConsentUiImpressionCount(int32_t count) = 0;
  virtual void AsyncWriteConsentStatus(
      quick_answers::prefs::ConsentStatus consent_status) = 0;
  virtual void AsyncWriteEnabled(bool enabled) = 0;

  // `FakeQuickAnswersState` overrides this method to fake feature type.
  virtual base::expected<FeatureType, Error> GetFeatureTypeExpected() const;

  // Set consent status of Quick Answers capability as a Quick Answers feature.
  void SetQuickAnswersFeatureConsentStatus(
      quick_answers::prefs::ConsentStatus consent_status);

  void SetIntentEligibilityAsQuickAnswers(quick_answers::Intent intent,
                                          bool eligible);

  void InitializeObserver(QuickAnswersStateObserver* observer);

  // Notify eligibility change to observers in the current feature type if it
  // has changed.
  void MaybeNotifyEligibilityChanged();
  void MaybeNotifyIsEnabledChanged();

  // Record the consent result with how many times the user has seen the consent
  // and impression duration.
  void RecordConsentResult(ConsentResultType type,
                           int nth_impression,
                           const base::TimeDelta duration);



  // The resolved application locale.
  std::string resolved_application_locale_;

  // The list of preferred languages, separated by comma.
  // (ex. "en-US,zh,fr").
  std::string preferred_languages_;

  // Whether the a11y spoken feedback tool is enabled.
  bool spoken_feedback_enabled_;

  // The number of times a user has seen the consent.
  int32_t consent_ui_impression_count_ = 0;

  // Whether the pref values has been initialized.
  bool prefs_initialized_ = false;

  // Whether to use text annotator for testing.
  bool use_text_annotator_for_testing_ = false;

  // Whether the Quick Answers is enabled in system settings.
  base::expected<bool, Error> quick_answers_enabled_ =
      base::unexpected(Error::kUninitialized);

  base::ObserverList<QuickAnswersStateObserver> observers_;

 private:
  void MaybeNotifyConsentStatusChanged();

  // Holds consent status of Quick Answers capability as a Quick Answers
  // feature.
  base::expected<quick_answers::prefs::ConsentStatus, Error>
      quick_answers_consent_status_ = base::unexpected(Error::kUninitialized);

  // Whether the definition is eligible as Quick Answers feature.
  base::expected<bool, Error> quick_answers_definition_eligible_ =
      base::unexpected(Error::kUninitialized);

  // Whether the translation is eligible as Quick Answers feature.
  base::expected<bool, Error> quick_answers_translation_eligible_ =
      base::unexpected(Error::kUninitialized);

  // Whether the unit conversion is eligible as Quick Answers feature.
  base::expected<bool, Error> quick_answers_unit_conversion_eligible_ =
      base::unexpected(Error::kUninitialized);

  // Use `base::expected` instead of `std::optional` to avoid implicit bool
  // conversion: https://abseil.io/tips/141.
  //
  // Dependencies:
  // - IsEligible <- ApplicationLocale
  // - IsEnabled <- IsEligible, GetConsentStatus
  // - GetConsentStatus <- none
  // - IsIntentEligible <- IsEligible
  //
  // Remember to call dependent values notify method if a value has changed,
  // e.g., call `MaybeNotifyIsEnabled` from `MaybeNotifyGetConsentStatus`.
  base::expected<bool, Error> IsEligibleExpected() const;
  base::expected<bool, Error> IsEligibleExpectedAs(
      FeatureType feature_type) const;
  base::expected<bool, Error> IsEnabledExpected() const;
  base::expected<bool, Error> IsEnabledExpectedAs(
      FeatureType feature_type) const;
  base::expected<quick_answers::prefs::ConsentStatus, Error>
  GetConsentStatusExpected() const;
  base::expected<quick_answers::prefs::ConsentStatus, Error>
  GetConsentStatusExpectedAs(FeatureType feature_type) const;
  base::expected<bool, Error> IsIntentEligibleExpected(
      quick_answers::Intent intent) const;
  base::expected<bool, Error> IsIntentEligibleExpectedAs(
      quick_answers::Intent intent,
      FeatureType feature_type) const;

  // Last notified values in the current feature type.
  base::expected<bool, Error> last_notified_is_eligible_ =
      base::unexpected(Error::kUninitialized);
  base::expected<bool, Error> last_notified_is_enabled_ =
      base::unexpected(Error::kUninitialized);
  base::expected<quick_answers::prefs::ConsentStatus, Error>
      last_notified_consent_status_ = base::unexpected(Error::kUninitialized);

  base::ScopedObservation<chromeos::MagicBoostState,
                          chromeos::MagicBoostState::Observer>
      magic_boost_state_observation_{this};

  // Test overwrite values.
  std::optional<bool> is_eligible_for_testing_;
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_STATE_H_
