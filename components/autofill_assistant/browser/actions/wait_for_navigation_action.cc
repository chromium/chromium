// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_navigation_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {
namespace {
constexpr base::TimeDelta kDefaultTimeout = base::TimeDelta::FromSeconds(20);
}  // namespace

WaitForNavigationAction::WaitForNavigationAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_wait_for_navigation());
}

WaitForNavigationAction::~WaitForNavigationAction() {}

void WaitForNavigationAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      proto_.wait_for_navigation().timeout_ms());
  if (timeout.is_zero())
    timeout = kDefaultTimeout;

  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&WaitForNavigationAction::OnTimeout,
                              weak_ptr_factory_.GetWeakPtr()));
  if (!delegate_->WaitForNavigation(
          base::BindOnce(&WaitForNavigationAction::OnWaitForNavigation,
                         weak_ptr_factory_.GetWeakPtr()))) {
    VLOG(1) << __func__
            << ": WaitForNavigation with no corresponding ExpectNavigation or "
               "Navigate";
    SendResult(INVALID_ACTION);
    return;
  }
}

void WaitForNavigationAction::OnWaitForNavigation(bool success) {
  if (!callback_) {
    // Already timed out.
    return;
  }

  SendResult(success ? ACTION_APPLIED : NAVIGATION_ERROR);
}

void WaitForNavigationAction::OnTimeout() {
  if (!callback_) {
    // Navigation has ended before.
    return;
  }

  if (delegate_->ExpectedNavigationHasStarted()) {
    // Navigation has started. Wait for it to end and to be reported to
    // OnWaitForNavigation.
    return;
  }
  SendResult(TIMED_OUT);
}

void WaitForNavigationAction::SendResult(ProcessedActionStatusProto status) {
  timer_.Stop();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
