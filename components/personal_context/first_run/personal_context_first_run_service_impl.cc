// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

namespace personal_context {

namespace {

bool AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
    PersonalContextEnablementService* enablement_service,
    PrefService* pref_service) {
  if (!enablement_service || !pref_service) {
    return false;
  }

  if (enablement_service->GetEnablementState() !=
      PersonalContextEnablementState::kEnabled) {
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
  pref_service->ClearPref(prefs::kPersonalContextAtMemoryNoticeShouldBeShown);
  pref_service->ClearPref(
      prefs::kPersonalContextInAutofillSettingsToggleStatus);
}

}  // namespace

PersonalContextFirstRunServiceImpl::PersonalContextFirstRunServiceImpl(
    std::unique_ptr<PersonalContextFirstRunClient> client,
    PersonalContextEnablementService* enablement_service,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : client_(std::move(client)),
      enablement_service_(enablement_service),
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
  }
}

void PersonalContextFirstRunServiceImpl::MaybeTriggerFirstRun(
    content::WebContents* web_contents,
    FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(FirstRunTriggerResult)> callback) {
  if (!enablement_service_ || !pref_service_) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }

  if (enablement_service_->GetEnablementState() !=
      PersonalContextEnablementState::kEnabled) {
    // Account not eligible.
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }

  // TODO(b:529716749): This part has insufficient test coverage. Investigate
  // in which capacity MaybeTriggerFirstRun() is still needed, and revamp it
  // and update test coverage accordingly.
  if (!pref_service_->GetBoolean(
          prefs::kPersonalContextInAutofillSettingsToggleStatus)) {
    // Disabled via toggle.
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }

  if (!pref_service_->GetBoolean(
          prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown)) {
    // Notice already shown.
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredAlreadyEnabled);
    return;
  }

  auto wrapped_callback = base::BindOnce(
      &PersonalContextFirstRunServiceImpl::OnNoticeDialogCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  client_->ShowNotice(web_contents, invocation_source,
                      std::move(wrapped_callback));
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() {
  if (pref_service_) {
    pref_service_->SetBoolean(
        prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  }
}

bool PersonalContextFirstRunServiceImpl::
    ShouldShowPersonalContextAmbientAutofillNotice() const {
  if (!AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
          enablement_service_, pref_service_)) {
    return false;
  }

  return pref_service_->GetBoolean(
             prefs::kPersonalContextInAutofillSettingsToggleStatus) &&
         pref_service_->GetBoolean(
             prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown);
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextInAtMemoryNoticeAsAcknowledged() {
  if (pref_service_) {
    // Acknowledging the AtMemory notice also counts as acknowledging the
    // Autofill notice.
    pref_service_->SetBoolean(
        prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
    pref_service_->SetBoolean(
        prefs::kPersonalContextAtMemoryNoticeShouldBeShown, false);
  }
}

bool PersonalContextFirstRunServiceImpl::
    ShouldShowPersonalContextAtMemoryNotice() const {
  if (!AreServicesAvailableAndAccountEligibleForPersonalIntelligence(
          enablement_service_, pref_service_)) {
    return false;
  }

  return pref_service_->GetBoolean(
             prefs::kPersonalContextInAutofillSettingsToggleStatus) &&
         pref_service_->GetBoolean(
             prefs::kPersonalContextAtMemoryNoticeShouldBeShown);
}

void PersonalContextFirstRunServiceImpl::OnNoticeDialogCompleted(
    base::OnceCallback<void(FirstRunTriggerResult)> callback,
    NoticeResult result) {
  if (result == NoticeResult::kAcknowledged) {
    if (pref_service_) {
      pref_service_->SetBoolean(
          prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
    }
  }
  std::move(callback).Run(FirstRunTriggerResult::kSuccess);
}

}  // namespace personal_context
