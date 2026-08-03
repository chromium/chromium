// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

namespace personal_context {

namespace {

bool AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
    PersonalContextEligibilityService* eligibility_service,
    PrefService* pref_service) {
  if (!eligibility_service || !pref_service) {
    return false;
  }

  if (eligibility_service->GetEligibilityState() !=
      PersonalContextEligibilityState::kEligible) {
    // Account not eligible.
    return false;
  }
  return true;
}

void ResetNoticePrefs(PrefService* pref_service) {
  if (!pref_service) {
    return;
  }
  pref_service->ClearPref(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown);
  pref_service->ClearPref(
      prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);
  pref_service->ClearPref(prefs::kPersonalContextAtMemoryNoticeShouldBeShown);
  pref_service->ClearPref(prefs::kPersonalContextAtMemoryNoticeImpressionCount);
  pref_service->ClearPref(
      prefs::kPersonalContextInAutofillSettingsToggleStatus);
}

}  // namespace

PersonalContextFirstRunServiceImpl::PersonalContextFirstRunServiceImpl(
    PersonalContextEligibilityService* eligibility_service,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : eligibility_service_(eligibility_service),
      pref_service_(pref_service),
      identity_manager_(identity_manager) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
  if (base::FeatureList::IsEnabled(
          features::debug::kPersonalContextResetNoticePrefsOnStartup)) {
    ResetNoticePrefs(pref_service_);
  }
}

PersonalContextFirstRunServiceImpl::~PersonalContextFirstRunServiceImpl() =
    default;

void PersonalContextFirstRunServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    ResetNoticePrefs(pref_service_);
    last_logged_ambient_autofill_session_id_ = std::nullopt;
    last_logged_at_memory_session_id_ = std::nullopt;
  }
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() {
  if (pref_service_) {
    int count = pref_service_->GetInteger(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);
    base::UmaHistogramCounts100(
        "PersonalContext.NoticeImpressionsBeforeAck.AmbientAutofill", count);
    pref_service_->ClearPref(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);

    pref_service_->SetBoolean(
        prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  }
}

bool PersonalContextFirstRunServiceImpl::
    ShouldShowPersonalContextAmbientAutofillNotice() const {
  if (!AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
          eligibility_service_, pref_service_)) {
    return false;
  }

  return pref_service_->GetBoolean(
             prefs::kPersonalContextInAutofillSettingsToggleStatus) &&
         pref_service_->GetBoolean(
             prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown);
}

void PersonalContextFirstRunServiceImpl::RecordAmbientAutofillNoticeImpression(
    uint32_t session_id) {
  if (pref_service_ && session_id != last_logged_ambient_autofill_session_id_) {
    int count = pref_service_->GetInteger(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);
    pref_service_->SetInteger(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount, count + 1);
    last_logged_ambient_autofill_session_id_ = session_id;
  }
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextInAtMemoryNoticeAsAcknowledged() {
  if (pref_service_) {
    const int count = pref_service_->GetInteger(
        prefs::kPersonalContextAtMemoryNoticeImpressionCount);
    base::UmaHistogramCounts100(
        "PersonalContext.NoticeImpressionsBeforeAck.AtMemory", count);
    pref_service_->ClearPref(
        prefs::kPersonalContextAtMemoryNoticeImpressionCount);

    // Acknowledging the AtMemory notice also counts as acknowledging the
    // Autofill notice. Record impressions before implicit acknowledgement
    // separately.
    const int ambient_count = pref_service_->GetInteger(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);
    base::UmaHistogramCounts100(
        "PersonalContext.NoticeImpressionsBeforeImplicitAck.AmbientAutofill",
        ambient_count);
    pref_service_->ClearPref(
        prefs::kPersonalContextAmbientAutofillNoticeImpressionCount);

    pref_service_->SetBoolean(
        prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
    pref_service_->SetBoolean(
        prefs::kPersonalContextAtMemoryNoticeShouldBeShown, false);
  }
}

bool PersonalContextFirstRunServiceImpl::
    ShouldShowPersonalContextAtMemoryNotice() const {
  if (!AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
          eligibility_service_, pref_service_)) {
    return false;
  }

  return pref_service_->GetBoolean(
             prefs::kPersonalContextInAutofillSettingsToggleStatus) &&
         pref_service_->GetBoolean(
             prefs::kPersonalContextAtMemoryNoticeShouldBeShown);
}

void PersonalContextFirstRunServiceImpl::RecordAtMemoryNoticeImpression(
    uint32_t session_id) {
  if (pref_service_ && session_id != last_logged_at_memory_session_id_) {
    int count = pref_service_->GetInteger(
        prefs::kPersonalContextAtMemoryNoticeImpressionCount);
    pref_service_->SetInteger(
        prefs::kPersonalContextAtMemoryNoticeImpressionCount, count + 1);
    last_logged_at_memory_session_id_ = session_id;
  }
}

}  // namespace personal_context
