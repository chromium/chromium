// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

#include <utility>

#include "base/functional/bind.h"

namespace accessibility_annotator {
AccessibilityAnnotatorFirstRunServiceImpl::
    AccessibilityAnnotatorFirstRunServiceImpl(
        std::unique_ptr<AccessibilityAnnotatorFirstRunClient> client)
    : client_(std::move(client)) {}

AccessibilityAnnotatorFirstRunServiceImpl::
    ~AccessibilityAnnotatorFirstRunServiceImpl() = default;

void AccessibilityAnnotatorFirstRunServiceImpl::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorFirstRunServiceImpl::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

RemoteAnnotatorEnablementState
AccessibilityAnnotatorFirstRunServiceImpl::GetEnablementState() {
  return current_state_;
}

void AccessibilityAnnotatorFirstRunServiceImpl::MaybeTriggerFirstRun(
    content::WebContents* web_contents,
    FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(FirstRunTriggerResult)> callback) {
  auto wrapped_callback = base::BindOnce(
      &AccessibilityAnnotatorFirstRunServiceImpl::OnInfoDialogCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // TODO(b/489414512): Check if and which first run should be
  // triggered.

  client_->ShowRemoteAnnotatorInfo(web_contents, invocation_source,
                                   std::move(wrapped_callback));
}

void AccessibilityAnnotatorFirstRunServiceImpl::OnInfoDialogCompleted(
    base::OnceCallback<void(FirstRunTriggerResult)> callback,
    InfoResult result) {
  if (result == InfoResult::kAcknowledged) {
    current_state_ = RemoteAnnotatorEnablementState::kEnabled;
    for (auto& observer : observers_) {
      observer.OnEnablementStateChanged(current_state_);
    }
  }
  std::move(callback).Run(FirstRunTriggerResult::kSuccess);
}

}  // namespace accessibility_annotator
