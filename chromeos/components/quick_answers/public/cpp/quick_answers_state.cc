// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

#include <cstdint>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

// Supported languages of the Quick Answers feature.
const std::string kSupportedLanguages[] = {"en", "es", "it", "fr", "pt", "de"};

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

QuickAnswersState::QuickAnswersState() {
  DCHECK(!g_quick_answers_state);
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

bool QuickAnswersState::IsSupportedLanguage(const std::string& language) {
  return base::Contains(kSupportedLanguages, language);
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_) {
    observer->OnPrefsInitialized();
    observer->OnSettingsEnabled(settings_enabled_);
    observer->OnConsentStatusUpdated(consent_status_);
    observer->OnApplicationLocaleReady(resolved_application_locale_);
    observer->OnPreferredLanguagesChanged(preferred_languages_);
    observer->OnEligibilityChanged(is_eligible_);
  }
}

void QuickAnswersState::UpdateEligibility() {
  if (resolved_application_locale_.empty())
    return;

  bool is_eligible =
      IsSupportedLanguage(l10n_util::GetLanguage(resolved_application_locale_));

  if (is_eligible_ == is_eligible)
    return;
  is_eligible_ = is_eligible;

  for (auto& observer : observers_) {
    observer.OnEligibilityChanged(is_eligible_);
  }
}
