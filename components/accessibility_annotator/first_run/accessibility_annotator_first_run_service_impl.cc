// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"

namespace accessibility_annotator {
AccessibilityAnnotatorFirstRunServiceImpl::
    AccessibilityAnnotatorFirstRunServiceImpl(
        std::unique_ptr<AccessibilityAnnotatorFirstRunClient> client,
        AccessibilityAnnotatorEnablementService* enablement_service)
    : client_(std::move(client)), enablement_service_(enablement_service) {}

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

  RemoteAnnotatorEnablementState state =
      enablement_service_->GetEnablementState();
  if (state == RemoteAnnotatorEnablementState::kDisabledNotEligible) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredNotEligible);
    return;
  }
  if (state == RemoteAnnotatorEnablementState::kEnabled) {
    std::move(callback).Run(FirstRunTriggerResult::kIgnoredAlreadyEnabled);
    return;
  }

  auto wrapped_callback = base::BindOnce(
      &AccessibilityAnnotatorFirstRunServiceImpl::OnInfoDialogCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  switch (state) {
    case RemoteAnnotatorEnablementState::kDisabledPendingInfo:
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
  // TODO(b/489414512): Update prefs when Info is acknowledged.
  if (result == InfoResult::kAcknowledged) {
  }
  std::move(callback).Run(FirstRunTriggerResult::kSuccess);
}

}  // namespace accessibility_annotator
