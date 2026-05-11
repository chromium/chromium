// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"

namespace accessibility_annotator {
AccessibilityAnnotatorFirstRunServiceImpl::
    AccessibilityAnnotatorFirstRunServiceImpl(
        std::unique_ptr<AccessibilityAnnotatorFirstRunClient> client,
        personal_context::PersonalContextEnablementService* enablement_service,
        PrefService* pref_service)
    : client_(std::move(client)),
      enablement_service_(enablement_service),
      pref_service_(pref_service) {}

AccessibilityAnnotatorFirstRunServiceImpl::
    ~AccessibilityAnnotatorFirstRunServiceImpl() = default;

void AccessibilityAnnotatorFirstRunServiceImpl::MaybeTriggerFirstRun(
    content::WebContents* web_contents,
    FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(FirstRunTriggerResult)> callback) {
  if (!enablement_service_) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }

  personal_context::PersonalContextEnablementState state =
      enablement_service_->GetEnablementState();
  if (state ==
      personal_context::PersonalContextEnablementState::kDisabledNotEligible) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }
  if (state == personal_context::PersonalContextEnablementState::kEnabled) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredAlreadyEnabled);
    return;
  }

  auto wrapped_callback = base::BindOnce(
      &AccessibilityAnnotatorFirstRunServiceImpl::OnInfoDialogCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  switch (state) {
    case personal_context::PersonalContextEnablementState::kDisabledPendingInfo:
      client_->ShowRemoteAnnotatorInfo(web_contents, invocation_source,
                                       std::move(wrapped_callback));
      break;
    default:
      break;
  }
}

void AccessibilityAnnotatorFirstRunServiceImpl::OnInfoDialogCompleted(
    base::OnceCallback<void(FirstRunTriggerResult)> callback,
    InfoResult result) {
  if (result == InfoResult::kAcknowledged) {
    if (pref_service_) {
      pref_service_->SetBoolean(
          personal_context::prefs::kShouldShowPersonalContextFirstRunInfo,
          false);
    }
  }
  std::move(callback).Run(FirstRunTriggerResult::kSuccess);
}

}  // namespace accessibility_annotator
