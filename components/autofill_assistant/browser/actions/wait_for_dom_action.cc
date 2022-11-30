// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_store.h"

namespace autofill_assistant {
namespace {

static constexpr base::TimeDelta kDefaultCheckDuration = base::Seconds(15);

void CollectExpectedElements(const ElementConditionProto& condition,
                             std::vector<std::string>* expected_client_ids) {
  switch (condition.type_case()) {
    case ElementConditionProto::kAllOf:
      for (const auto& inner_condition : condition.all_of().conditions()) {
        CollectExpectedElements(inner_condition, expected_client_ids);
      }
      break;
    case ElementConditionProto::kAnyOf:
      for (const auto& inner_condition : condition.any_of().conditions()) {
        CollectExpectedElements(inner_condition, expected_client_ids);
      }
      break;
    case ElementConditionProto::kNoneOf:
      for (const auto& inner_condition : condition.none_of().conditions()) {
        CollectExpectedElements(inner_condition, expected_client_ids);
      }
      break;
    case ElementConditionProto::kMatch:
      if (condition.has_client_id()) {
        expected_client_ids->emplace_back(condition.client_id().identifier());
      }
      break;
    case ElementConditionProto::TYPE_NOT_SET:
      break;
  }
}

}  // namespace

WaitForDomAction::WaitForDomAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {}

WaitForDomAction::~WaitForDomAction() {}

void WaitForDomAction::InternalProcessAction(ProcessActionCallback callback) {
  base::TimeDelta max_wait_time = kDefaultCheckDuration;
  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    max_wait_time = base::Milliseconds(timeout_ms);

  if (!proto_.wait_for_dom().has_wait_condition()) {
    VLOG(2) << "WaitForDomAction: no condition specified";
    ReportActionResult(std::move(callback), ClientStatus(INVALID_ACTION));
    return;
  }
  delegate_->WaitForDomWithSlowWarning(
      max_wait_time, proto_.wait_for_dom().allow_interrupt(),
      /* observer= */ nullptr,
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
  checker->AddElementConditionCheck(
      proto_.wait_for_dom().wait_condition(),
      base::BindOnce(&WaitForDomAction::OnWaitConditionDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::OnWaitConditionDone(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    const std::vector<std::string>& payloads,
    const std::vector<std::string>& tags,
    const base::flat_map<std::string, DomObjectFrameStack>& elements) {
  // Results are first cleared, as OnWaitConditionDone can be called more
  // than once. Yet, we want report only the payloads sent with the final call
  // to OnWaitConditionDone() as action result.

  auto* result = processed_action_proto_->mutable_wait_for_dom_result();
  result->clear_matching_condition_payloads();
  for (const std::string& payload : payloads) {
    result->add_matching_condition_payloads(payload);
  }
  for (const std::string& tag : tags) {
    result->add_matching_condition_tags(tag);
  }

  elements_ = elements;

  std::move(callback).Run(status);
}

void WaitForDomAction::UpdateElementStore() {
  std::vector<std::string> expected_client_ids;
  CollectExpectedElements(proto_.wait_for_dom().wait_condition(),
                          &expected_client_ids);

  auto* store = delegate_->GetElementStore();
  for (const auto& client_id : expected_client_ids) {
    store->RemoveElement(client_id);
  }
  for (const auto& it : elements_) {
    store->AddElement(it.first, it.second);
  }
}

void WaitForDomAction::ReportActionResult(ProcessActionCallback callback,
                                          const ClientStatus& status) {
  UpdateElementStore();
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
