// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace {
static constexpr base::TimeDelta kDefaultCheckDuration =
    base::TimeDelta::FromSeconds(15);
}  // namespace

namespace autofill_assistant {

WaitForDomAction::WaitForDomAction(const ActionProto& proto) : Action(proto) {}

WaitForDomAction::~WaitForDomAction() {}

void WaitForDomAction::InternalProcessAction(ActionDelegate* delegate,
                                             ProcessActionCallback callback) {
  DCHECK_GT(proto_.wait_for_dom().selectors_size(), 0);
  batch_element_checker_ = delegate->CreateBatchElementChecker();
  batch_element_checker_->AddElementCheck(
      kVisibilityCheck, ExtractSelectors(proto_.wait_for_dom().selectors()),
      base::DoNothing());

  base::TimeDelta duration = kDefaultCheckDuration;
  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    duration = base::TimeDelta::FromMilliseconds(timeout_ms);

  batch_element_checker_->Run(
      duration,
      /* try_done= */ base::DoNothing(),
      /* all_done= */
      base::BindOnce(&WaitForDomAction::OnCheckDone,
                     // batch_element_checker_ is owned by this
                     base::Unretained(this), std::move(callback)));
}

void WaitForDomAction::OnCheckDone(ProcessActionCallback callback) {
  UpdateProcessedAction(batch_element_checker_->all_found()
                            ? ACTION_APPLIED
                            : ELEMENT_RESOLUTION_FAILED);
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
