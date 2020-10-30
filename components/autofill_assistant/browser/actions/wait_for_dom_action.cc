// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace {
static constexpr base::TimeDelta kDefaultCheckDuration =
    base::TimeDelta::FromSeconds(15);
}  // namespace

namespace autofill_assistant {

WaitForDomAction::WaitForDomAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {}

WaitForDomAction::~WaitForDomAction() {}

void WaitForDomAction::InternalProcessAction(ProcessActionCallback callback) {
  base::TimeDelta max_wait_time = kDefaultCheckDuration;
  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    max_wait_time = base::TimeDelta::FromMilliseconds(timeout_ms);

  if (!proto_.wait_for_dom().has_wait_condition()) {
    VLOG(2) << "WaitForDomAction: no condition specified";
    ReportActionResult(std::move(callback), ClientStatus(INVALID_ACTION));
    return;
  }
  wait_condition_ = std::make_unique<ElementPrecondition>(
      proto_.wait_for_dom().wait_condition());
  delegate_->WaitForDom(
      max_wait_time, proto_.wait_for_dom().allow_interrupt(),
      base::BindRepeating(&WaitForDomAction::CheckElements,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &WaitForDomAction::OnWaitForElementTimed,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&WaitForDomAction::ReportActionResult,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void WaitForDomAction::CheckElements(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  wait_condition_->Check(
      checker,
      base::BindOnce(&WaitForDomAction::OnWaitConditionDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::OnWaitConditionDone(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    const std::vector<std::string>& payloads) {
  auto* result = processed_action_proto_->mutable_wait_for_dom_result();
  // Conditions are first cleared, as OnWaitConditionDone can be called more
  // than once. Yet, we want report only the payloads sent with the final call
  // to OnWaitConditionDone() as action result.
  result->clear_matching_condition_payloads();
  for (const std::string& payload : payloads) {
    result->add_matching_condition_payloads(payload);
  }
  std::move(callback).Run(status);
}

void WaitForDomAction::ReportActionResult(ProcessActionCallback callback,
                                          const ClientStatus& status) {
  UpdateProcessedAction(status.proto_status());
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
