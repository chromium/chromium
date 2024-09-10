// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include <cstdint>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/types/expected.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

// Supported languages of the Quick Answers feature.
constexpr auto kSupportedLanguages =
    base::MakeFixedFlatSet<std::string>({"en", "es", "it", "fr", "pt", "de"});

// TODO(b/340628526): extract Error and ConsentStatus enums to a shared place.
QuickAnswersState::Error ToQuickAnswersStateError(
    chromeos::MagicBoostState::Error error) {
  switch (error) {
    case chromeos::MagicBoostState::Error::kUninitialized:
      return QuickAnswersState::Error::kUninitialized;
  }

  CHECK(false) << "Unknown MagicBoostState::Error enum class value provided.";
}

quick_answers::prefs::ConsentStatus ToQuickAnswersPrefsConsentStatus(
    chromeos::HMRConsentStatus consent_status) {
  switch (consent_status) {
    case chromeos::HMRConsentStatus::kUnset:
      return quick_answers::prefs::ConsentStatus::kUnknown;
    case chromeos::HMRConsentStatus::kPendingDisclaimer:
      // Quick Answers capability is available from `kPendingDisclaimer` state.
      // See comments in `chromeos::HMRConsentStatus` for details of those
      // states.
      return quick_answers::prefs::ConsentStatus::kAccepted;
    case chromeos::HMRConsentStatus::kApproved:
      return quick_answers::prefs::ConsentStatus::kAccepted;
    case chromeos::HMRConsentStatus::kDeclined:
      return quick_answers::prefs::ConsentStatus::kRejected;
  }

  CHECK(false) << "Unknown HMRConsentStatus enum class value provided.";
}

base::expected<bool, QuickAnswersState::Error> ToQuickAnswersStateIsEnabled(
    base::expected<bool, chromeos::MagicBoostState::Error> is_enabled) {
  return is_enabled.transform_error(&ToQuickAnswersStateError);
}

base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
ToQuickAnswersStateConsentStatus(
    base::expected<chromeos::HMRConsentStatus, chromeos::MagicBoostState::Error>
        consent_status) {
  return consent_status.transform_error(&ToQuickAnswersStateError)
      .transform(&ToQuickAnswersPrefsConsentStatus);
}

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

// static
QuickAnswersState::FeatureType QuickAnswersState::GetFeatureType() {
  QuickAnswersState* quick_answers_state = Get();
  if (!quick_answers_state) {
    return QuickAnswersState::FeatureType::kQuickAnswers;
  }

  return quick_answers_state->GetFeatureTypeExpected().value_or(
      QuickAnswersState::FeatureType::kQuickAnswers);
}

// static
bool QuickAnswersState::IsEligible() {
  return IsEligibleAs(GetFeatureType());
}

// static
bool QuickAnswersState::IsEligibleAs(
    QuickAnswersState::FeatureType feature_type) {
  QuickAnswersState* quick_answers_state = Get();
  if (!quick_answers_state) {
    return false;
  }

  return quick_answers_state->IsEligibleExpectedAs(feature_type)
      .value_or(false);
}

// static
bool QuickAnswersState::IsEnabled() {
  return IsEnabledAs(GetFeatureType());
}

// static
bool QuickAnswersState::IsEnabledAs(FeatureType feature_type) {
  QuickAnswersState* quick_answers_state = Get();
  if (!quick_answers_state) {
    return false;
  }

  return quick_answers_state->IsEnabledExpectedAs(feature_type).value_or(false);
}

// static
base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
QuickAnswersState::GetConsentStatus() {
  return GetConsentStatusAs(GetFeatureType());
}

// static
base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
QuickAnswersState::GetConsentStatusAs(FeatureType feature_type) {
  QuickAnswersState* quick_answers_state = Get();
  if (!quick_answers_state) {
    return base::unexpected(QuickAnswersState::Error::kUninitialized);
  }

  return quick_answers_state->GetConsentStatusExpectedAs(feature_type);
}

// static
bool QuickAnswersState::IsIntentEligible(quick_answers::Intent intent) {
  return IsIntentEligibleAs(intent, GetFeatureType());
}

// static
bool QuickAnswersState::IsIntentEligibleAs(quick_answers::Intent intent,
                                           FeatureType feature_type) {
  QuickAnswersState* quick_answers_state = Get();
  if (!quick_answers_state) {
    return false;
  }

  return quick_answers_state->IsIntentEligibleExpectedAs(intent, feature_type)
      .value_or(false);
}

QuickAnswersState::QuickAnswersState() {
  CHECK(!g_quick_answers_state);
  g_quick_answers_state = this;

  if (GetFeatureType() == FeatureType::kHmr) {
    chromeos::MagicBoostState* magic_boost_state =
        chromeos::MagicBoostState::Get();
    CHECK(magic_boost_state) << "QuickAnswersState depends on MagicBoostState.";
    magic_boost_state_observation_.Observe(magic_boost_state);
  }
}

QuickAnswersState::~QuickAnswersState() {
  DCHECK_EQ(g_quick_answers_state, this);
  g_quick_answers_state = nullptr;
}

void QuickAnswersState::AddObserver(QuickAnswersStateObserver* observer) {
  observers_.AddObserver(observer);
  InitializeObserver(observer);
}

void QuickAnswersState::RemoveObserver(QuickAnswersStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void QuickAnswersState::OnHMREnabledUpdated(bool enabled) {
  MaybeNotifyIsEnabledChanged();
}

void QuickAnswersState::OnHMRConsentStatusUpdated(
    chromeos::HMRConsentStatus consent_status) {
  MaybeNotifyConsentStatusChanged();
}

void QuickAnswersState::OnIsDeleting() {
  magic_boost_state_observation_.Reset();
}

void QuickAnswersState::AsyncSetConsentStatus(
    quick_answers::prefs::ConsentStatus consent_status) {
  switch (consent_status) {
    case quick_answers::prefs::ConsentStatus::kAccepted:
      AsyncWriteConsentStatus(quick_answers::prefs::ConsentStatus::kAccepted);
      AsyncWriteEnabled(true);
      break;
    case quick_answers::prefs::ConsentStatus::kRejected:
      AsyncWriteConsentStatus(quick_answers::prefs::ConsentStatus::kRejected);
      AsyncWriteEnabled(false);
      break;
    case quick_answers::prefs::ConsentStatus::kUnknown:
      // This is test only path for now. `kUnknown` is set only from default
      // values in prod.
      CHECK_IS_TEST();

      AsyncWriteConsentStatus(quick_answers::prefs::ConsentStatus::kUnknown);
      AsyncWriteEnabled(false);
      break;
  }
}

int32_t QuickAnswersState::AsyncIncrementImpressionCount() {
  int32_t incremented_count = consent_ui_impression_count_ + 1;
  AsyncWriteConsentUiImpressionCount(incremented_count);
  return incremented_count;
}

bool QuickAnswersState::ShouldUseQuickAnswersTextAnnotator() {
  return base::SysInfo::IsRunningOnChromeOS() ||
         use_text_annotator_for_testing_;
}

bool QuickAnswersState::IsSupportedLanguage(const std::string& language) const {
  return kSupportedLanguages.contains(language);
}

base::expected<QuickAnswersState::FeatureType, QuickAnswersState::Error>
QuickAnswersState::GetFeatureTypeExpected() const {
  chromeos::MagicBoostState* magic_boost_state =
      chromeos::MagicBoostState::Get();
  if (!magic_boost_state) {
    // `magic_boost_state` might be null in tests
    return base::unexpected(QuickAnswersState::Error::kUninitialized);
  }

  return magic_boost_state->IsMagicBoostAvailable()
             ? QuickAnswersState::FeatureType::kHmr
             : QuickAnswersState::FeatureType::kQuickAnswers;
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsEligibleExpected() const {
  return IsEligibleExpectedAs(GetFeatureType());
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsEligibleExpectedAs(
    QuickAnswersState::FeatureType feature_type) const {
  if (is_eligible_for_testing_.has_value()) {
    CHECK_IS_TEST();
    return is_eligible_for_testing_.value();
  }

  if (GetFeatureType() != feature_type) {
    return false;
  }

  if (resolved_application_locale_.empty()) {
    return base::unexpected(QuickAnswersState::Error::kUninitialized);
  }

  return IsSupportedLanguage(
      l10n_util::GetLanguage(resolved_application_locale_));
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsEnabledExpected() const {
  return IsEnabledExpectedAs(GetFeatureType());
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsEnabledExpectedAs(
    QuickAnswersState::FeatureType feature_type) const {
  // TODO(b/340628526): Use `IsEligibleExpectedAs` to propagate error values.
  if (!IsEligibleAs(feature_type)) {
    return false;
  }

  // Quick Answers capability should not be enabled without kAccepted consent
  // status. Note that there is a combination of IsEnabled=false and
  // ConsentStatus=kAccepted if a user has turned off a feature after they have
  // enabled/consented.
  // TODO(b/340628526): Use `GetConsentStatusExpectedAs` to propagate error
  // values.
  if (GetConsentStatusAs(feature_type) !=
      quick_answers::prefs::ConsentStatus::kAccepted) {
    return false;
  }

  switch (feature_type) {
    case QuickAnswersState::FeatureType::kHmr: {
      chromeos::MagicBoostState* magic_boost_state =
          chromeos::MagicBoostState::Get();
      if (!magic_boost_state) {
        return base::unexpected(QuickAnswersState::Error::kUninitialized);
      }

      return ToQuickAnswersStateIsEnabled(magic_boost_state->hmr_enabled());
    }
    case QuickAnswersState::FeatureType::kQuickAnswers: {
      if (chromeos::IsKioskSession()) {
        return false;
      }

      return quick_answers_enabled_;
    }
  }
}

base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
QuickAnswersState::GetConsentStatusExpected() const {
  return GetConsentStatusExpectedAs(GetFeatureType());
}

base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
QuickAnswersState::GetConsentStatusExpectedAs(
    QuickAnswersState::FeatureType feature_type) const {
  switch (feature_type) {
    case QuickAnswersState::FeatureType::kHmr: {
      chromeos::MagicBoostState* magic_boost_state =
          chromeos::MagicBoostState::Get();
      if (!magic_boost_state) {
        return base::unexpected(QuickAnswersState::Error::kUninitialized);
      }

      return ToQuickAnswersStateConsentStatus(
          magic_boost_state->hmr_consent_status());
    }
    case QuickAnswersState::FeatureType::kQuickAnswers: {
      return quick_answers_consent_status_;
    }
  }
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsIntentEligibleExpected(
    quick_answers::Intent intent) const {
  return IsIntentEligibleExpectedAs(intent, GetFeatureType());
}

base::expected<bool, QuickAnswersState::Error>
QuickAnswersState::IsIntentEligibleExpectedAs(
    quick_answers::Intent intent,
    QuickAnswersState::FeatureType feature_type) const {
  // Use `IsEligibleExpectedAs` instead of `IsEligibleAs` since we would like to
  // return an error value if eligible is an error value.
  base::expected<bool, QuickAnswersState::Error> maybe_eligible =
      IsEligibleExpectedAs(feature_type);
  if (maybe_eligible != true) {
    return maybe_eligible;
  }

  switch (feature_type) {
    case QuickAnswersState::FeatureType::kHmr:
      // All intents are always eligible for kHmr.
      return true;
    case QuickAnswersState::FeatureType::kQuickAnswers:
      switch (intent) {
        case quick_answers::Intent::kDefinition:
          return quick_answers_definition_eligible_;
        case quick_answers::Intent::kTranslation:
          return quick_answers_translation_eligible_;
        case quick_answers::Intent::kUnitConversion:
          return quick_answers_unit_conversion_eligible_;
      }

      CHECK(false) << "Invalid IntentType enum class value provided.";
  }
}

void QuickAnswersState::SetEligibilityForTesting(bool is_eligible) {
  CHECK_IS_TEST();
  is_eligible_for_testing_ = is_eligible;
  MaybeNotifyEligibilityChanged();
}

void QuickAnswersState::SetQuickAnswersFeatureConsentStatus(
    quick_answers::prefs::ConsentStatus consent_status) {
  quick_answers_consent_status_ = consent_status;

  MaybeNotifyConsentStatusChanged();
}

void QuickAnswersState::SetIntentEligibilityAsQuickAnswers(
    quick_answers::Intent intent,
    bool eligible) {
  switch (intent) {
    case quick_answers::Intent::kDefinition:
      quick_answers_definition_eligible_ = eligible;
      return;
    case quick_answers::Intent::kTranslation:
      quick_answers_translation_eligible_ = eligible;
      return;
    case quick_answers::Intent::kUnitConversion:
      quick_answers_unit_conversion_eligible_ = eligible;
      return;
  }

  CHECK(false) << "Invalid Intent enum class value provided.";
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_) {
    observer->OnPrefsInitialized();
    observer->OnApplicationLocaleReady(resolved_application_locale_);
    observer->OnPreferredLanguagesChanged(preferred_languages_);
  }

  base::expected<bool, QuickAnswersState::Error> maybe_is_enabled =
      IsEnabledExpected();
  if (maybe_is_enabled.has_value()) {
    observer->OnSettingsEnabled(maybe_is_enabled.value());
  }

  base::expected<bool, QuickAnswersState::Error> maybe_is_eligible =
      IsEligibleExpected();
  if (maybe_is_eligible.has_value()) {
    observer->OnEligibilityChanged(maybe_is_eligible.value());
  }

  base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
      maybe_consent_status = GetConsentStatusExpected();
  if (maybe_consent_status.has_value()) {
    observer->OnConsentStatusUpdated(maybe_consent_status.value());
  }
}

void QuickAnswersState::MaybeNotifyEligibilityChanged() {
  base::expected<bool, QuickAnswersState::Error> is_eligible =
      IsEligibleExpected();

  if (last_notified_is_eligible_ == is_eligible) {
    return;
  }

  last_notified_is_eligible_ = is_eligible;

  if (!last_notified_is_eligible_.has_value()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnEligibilityChanged(last_notified_is_eligible_.value());
  }

  // `IsEnabled` depends on `IsEligible`. Maybe notify if eligibility has
  // changed.
  MaybeNotifyIsEnabledChanged();
}

void QuickAnswersState::MaybeNotifyIsEnabledChanged() {
  base::expected<bool, QuickAnswersState::Error> is_enabled =
      IsEnabledExpected();

  if (last_notified_is_enabled_ == is_enabled) {
    return;
  }

  last_notified_is_enabled_ = is_enabled;

  if (!last_notified_is_enabled_.has_value()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnSettingsEnabled(last_notified_is_enabled_.value());
  }
}

void QuickAnswersState::MaybeNotifyConsentStatusChanged() {
  base::expected<quick_answers::prefs::ConsentStatus, Error> consent_status =
      GetConsentStatusExpected();

  if (last_notified_consent_status_ == consent_status) {
    return;
  }

  last_notified_consent_status_ = consent_status;

  // TODO(b/340628526): Change other MaybeNotify methods to notify a value
  // change to dependent values for error value case.
  if (last_notified_consent_status_.has_value()) {
    for (auto& observer : observers_) {
      observer.OnConsentStatusUpdated(last_notified_consent_status_.value());
    }
  }

  // `IsEnabled` depends on `GetConsentStatus`. Maybe notify if consent status
  // has changed.
  MaybeNotifyIsEnabledChanged();
}
