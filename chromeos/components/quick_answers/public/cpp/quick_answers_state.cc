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
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

// Supported languages of the Quick Answers feature.
constexpr auto kSupportedLanguages =
    base::MakeFixedFlatSet<std::string>({"en", "es", "it", "fr", "pt", "de"});

QuickAnswersState::FeatureType GetFeatureType() {
  return chromeos::features::IsMagicBoostEnabled()
             ? QuickAnswersState::FeatureType::kHmr
             : QuickAnswersState::FeatureType::kQuickAnswers;
}

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
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

QuickAnswersState::QuickAnswersState() {
  CHECK(!g_quick_answers_state);
  g_quick_answers_state = this;
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

void QuickAnswersState::SetEligibilityForTesting(bool is_eligible) {
  CHECK_IS_TEST();
  is_eligible_for_testing_ = is_eligible;
  MaybeNotifyEligibilityChanged();
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_) {
    observer->OnPrefsInitialized();
    observer->OnSettingsEnabled(settings_enabled_);
    observer->OnConsentStatusUpdated(consent_status_);
    observer->OnApplicationLocaleReady(resolved_application_locale_);
    observer->OnPreferredLanguagesChanged(preferred_languages_);
  }

  base::expected<bool, QuickAnswersState::Error> maybe_is_eligible =
      IsEligibleExpected();
  if (maybe_is_eligible.has_value()) {
    observer->OnEligibilityChanged(maybe_is_eligible.value());
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
}
