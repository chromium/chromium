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

  AddConditionsFromProto();
  if (conditions_.empty()) {
    DVLOG(2) << "WaitForDomAction: no selectors specified";
    OnCheckDone(std::move(callback), ClientStatus(INVALID_ACTION));
    return;
  }
  for (size_t i = 0; i < conditions_.size(); i++) {
    if (conditions_[i].selector.empty()) {
      DVLOG(2) << "WaitForDomAction: selector for condition " << i
               << " is empty";
      OnCheckDone(std::move(callback), ClientStatus(INVALID_SELECTOR));
      return;
    }
  }

  delegate_->WaitForDom(
      max_wait_time, proto_.wait_for_dom().allow_interrupt(),
      base::BindRepeating(&WaitForDomAction::CheckElements,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WaitForDomAction::OnCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::AddConditionsFromProto() {
  require_all_ = false;
  switch (proto_.wait_for_dom().wait_on_case()) {
    case WaitForDomProto::kWaitUntil:
      AddCondition(SelectorPredicate::kMatch,
                   proto_.wait_for_dom().wait_until(), "");
      break;

    case WaitForDomProto::kWaitWhile:
      AddCondition(SelectorPredicate::kNoMatch,
                   proto_.wait_for_dom().wait_while(), "");
      break;

    case WaitForDomProto::kWaitForAll:
      require_all_ = true;
      for (const auto& condition_proto :
           proto_.wait_for_dom().wait_for_all().conditions()) {
        AddCondition(condition_proto);
      }
      break;

    case WaitForDomProto::kWaitForAny:
      for (const auto& condition_proto :
           proto_.wait_for_dom().wait_for_any().conditions()) {
        AddCondition(condition_proto);
      }
      break;

    case WaitForDomProto::WAIT_ON_NOT_SET:
      // an empty condition_ will be rejected when validating conditions_.
      break;

      // No default set to trigger a compilation error if a new case is added.
  }
}

void WaitForDomAction::AddCondition(
    const WaitForDomProto::ElementCondition& condition) {
  if (condition.has_must_match()) {
    AddCondition(SelectorPredicate::kMatch, condition.must_match(),
                 condition.server_payload());
  } else {
    AddCondition(SelectorPredicate::kNoMatch, condition.must_not_match(),
                 condition.server_payload());
  }
}

void WaitForDomAction::AddCondition(SelectorPredicate predicate,
                                    const ElementReferenceProto& selector_proto,
                                    const std::string& server_payload) {
  conditions_.emplace_back();
  Condition& condition = conditions_.back();
  condition.predicate = predicate;
  condition.selector = Selector(selector_proto);
  condition.server_payload = server_payload;
}

void WaitForDomAction::CheckElements(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  for (size_t i = 0; i < conditions_.size(); i++) {
    checker->AddElementCheck(
        conditions_[i].selector,
        base::BindOnce(&WaitForDomAction::OnSingleElementCheckDone,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  checker->AddAllDoneCallback(
      base::BindOnce(&WaitForDomAction::OnAllElementChecksDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::OnSingleElementCheckDone(
    size_t condition_index,
    const ClientStatus& element_status) {
  DCHECK(condition_index < conditions_.size());
  Condition& condition = conditions_[condition_index];
  condition.status_proto = element_status.proto_status();
  condition.match = condition.predicate == SelectorPredicate::kMatch
                        ? element_status.ok()
                        : !element_status.ok();
  // We can't stop here since the batch checker does not support stopping from a
  // single element callback.
}

void WaitForDomAction::OnAllElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  size_t match_count = 0;
  ProcessedActionStatusProto last_error = UNKNOWN_ACTION_STATUS;
  for (auto& condition : conditions_) {
    if (condition.match) {
      match_count++;
    } else {
      switch (condition.predicate) {
        case SelectorPredicate::kMatch:
          // For an expected match, return the status.
          last_error = condition.status_proto;
          break;
        case SelectorPredicate::kNoMatch:
          // For an expected non-match, return an ELEMENT_RESOLUTION_FAILED for
          // lack of better status. We're expecting the element to not be there,
          // but it is.
          last_error = ELEMENT_RESOLUTION_FAILED;
          break;

          // Intentionally no default case to make compilation fail if a new
          // value was added to the enum that is not treated not here.
      }
    }
  }
  bool success =
      require_all_ ? match_count == conditions_.size() : match_count > 0;
  std::move(callback).Run(success ? OkClientStatus()
                                  : ClientStatus(last_error));
}

void WaitForDomAction::OnCheckDone(ProcessActionCallback callback,
                                   const ClientStatus& status) {
  UpdateProcessedAction(status.proto_status());
  for (auto& condition : conditions_) {
    if (condition.match && !condition.server_payload.empty()) {
      processed_action_proto_->mutable_wait_for_dom_result()
          ->add_matching_condition_payloads(condition.server_payload);
    }
  }
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
