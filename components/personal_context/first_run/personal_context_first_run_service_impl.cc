// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

namespace personal_context {
PersonalContextFirstRunServiceImpl::PersonalContextFirstRunServiceImpl(
    std::unique_ptr<PersonalContextFirstRunClient> client,
    PersonalContextEnablementService* enablement_service,
    PrefService* pref_service)
    : client_(std::move(client)),
      enablement_service_(enablement_service),
      pref_service_(pref_service) {}

PersonalContextFirstRunServiceImpl::~PersonalContextFirstRunServiceImpl() =
    default;

void PersonalContextFirstRunServiceImpl::MaybeTriggerFirstRun(
    content::WebContents* web_contents,
    FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(FirstRunTriggerResult)> callback) {
  if (!enablement_service_) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }

  PersonalContextEnablementState state =
      enablement_service_->GetEnablementState();
  if (state == PersonalContextEnablementState::kDisabledNotEligible) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }
  if (state == PersonalContextEnablementState::kEnabled) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredAlreadyEnabled);
    return;
  }

  auto wrapped_callback = base::BindOnce(
      &PersonalContextFirstRunServiceImpl::OnNoticeDialogCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  switch (state) {
    case PersonalContextEnablementState::kDisabledShouldShowNotice:
      client_->ShowNotice(web_contents, invocation_source,
                          std::move(wrapped_callback));
      break;
    default:
      break;
  }
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextInAutofillNoticeAsShown() {
  if (pref_service_) {
    // The notice being shown should enable the feature.
    pref_service_->SetBoolean(
        prefs::kPersonalContextInAutofillNoticeHasBeenShown, true);
    pref_service_->SetBoolean(
        prefs::kPersonalContextInAutofillSettingsToggleStatus, true);
  }
}

void PersonalContextFirstRunServiceImpl::
    MarkPersonalContextInAutofillNoticeAsAcknowledged() {
  if (pref_service_) {
    pref_service_->SetBoolean(
        prefs::kPersonalContextInAutofillSettingsToggleStatus, true);
    // It's guaranteed the notice was shown when the user acknowledges it, hence
    // no need to manually set the kPersonalContextInAutofillNoticeHasBeenShown
    // pref.
    pref_service_->SetBoolean(
        prefs::kPersonalContextInAutofillNoticeShouldBeShown, false);
  }
}

bool PersonalContextFirstRunServiceImpl::
    ShouldShowPersonalContextAutofillNotice() const {
  if (!features::IsPersonalContextFirstRunNoticePhase2Enabled()) {
    return false;
  }
  if (!enablement_service_) {
    return false;
  }

  PersonalContextEnablementState state =
      enablement_service_->GetEnablementState();
  return state == PersonalContextEnablementState::kDisabledShouldShowNotice ||
         state == PersonalContextEnablementState::kEnabledShouldShowNotice;
}

void PersonalContextFirstRunServiceImpl::OnNoticeDialogCompleted(
    base::OnceCallback<void(FirstRunTriggerResult)> callback,
    NoticeResult result) {
  if (result == NoticeResult::kAcknowledged) {
    if (pref_service_) {
      pref_service_->SetBoolean(
          prefs::kPersonalContextInAutofillNoticeShouldBeShown, false);
    }
  }
  std::move(callback).Run(FirstRunTriggerResult::kSuccess);
}

}  // namespace personal_context
