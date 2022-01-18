// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

BatchElementChecker::BatchElementChecker() = default;

BatchElementChecker::~BatchElementChecker() = default;

BatchElementChecker::Result::Result() = default;

BatchElementChecker::Result::~Result() = default;

BatchElementChecker::Result::Result(const Result&) = default;

BatchElementChecker::ElementConditionCheck::ElementConditionCheck() = default;

BatchElementChecker::ElementConditionCheck::~ElementConditionCheck() = default;
BatchElementChecker::ElementConditionCheck::ElementConditionCheck(
    ElementConditionCheck&&) = default;

void BatchElementChecker::AddElementCheck(const Selector& selector,
                                          bool strict,
                                          ElementCheckCallback callback) {
  DCHECK(!started_);

  element_check_callbacks_[std::make_pair(selector, strict)].emplace_back(
      std::move(callback));
}

void BatchElementChecker::AddElementConditionCheck(
    const ElementConditionProto& condition,
    ElementConditionCheckCallback callback) {
  DCHECK(!started_);
  if (IsElementConditionEmpty(condition)) {
    std::move(callback).Run(ClientStatus(ACTION_APPLIED), {}, {});
    return;
  }

  BatchElementChecker::ElementConditionCheck check;
  check.proto = condition;
  check.callback = std::move(callback);
  element_condition_checks_.emplace_back(std::move(check));
  size_t index = element_condition_checks_.size() - 1;
  AddElementConditionResults(element_condition_checks_[index].proto, index);
}

void BatchElementChecker::AddFieldValueCheck(const Selector& selector,
                                             GetFieldValueCallback callback) {
  DCHECK(!started_);

  get_field_value_callbacks_[selector].emplace_back(std::move(callback));
}

bool BatchElementChecker::empty() const {
  return element_check_callbacks_.empty() && get_field_value_callbacks_.empty();
}

bool BatchElementChecker::IsElementConditionEmpty(
    const ElementConditionProto& proto) {
  return proto.type_case() == ElementConditionProto::TYPE_NOT_SET;
}

void BatchElementChecker::AddAllDoneCallback(
    base::OnceCallback<void()> all_done) {
  all_done_.emplace_back(std::move(all_done));
}

void BatchElementChecker::Run(WebController* web_controller) {
  DCHECK(web_controller);
  DCHECK(!started_);
  started_ = true;

  pending_checks_count_ =
      element_check_callbacks_.size() + get_field_value_callbacks_.size() + 1;

  for (auto& entry : element_check_callbacks_) {
    web_controller->FindElement(
        /* selector= */ entry.first.first, /* strict= */ entry.first.second,
        base::BindOnce(
            &BatchElementChecker::OnElementChecked,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
  }

  for (auto& entry : get_field_value_callbacks_) {
    web_controller->FindElement(
        entry.first, /* strict= */ true,
        base::BindOnce(
            &element_action_util::TakeElementAndGetProperty<const std::string&>,
            base::BindOnce(&WebController::GetFieldValue,
                           web_controller->GetWeakPtr()),
            std::string(),
            base::BindOnce(&BatchElementChecker::OnFieldValueChecked,
                           weak_ptr_factory_.GetWeakPtr(),
                           // Guaranteed to exist for the lifetime of
                           // this instance, because the map isn't
                           // modified after Run has been called.
                           base::Unretained(&entry.second))));
  }

  // The extra +1 of pending_check_count and this check happening last
  // guarantees that all_done cannot be called before the end of this function.
  // Without this, callbacks could be called synchronously by the web
  // controller, the call all_done, which could delete this instance and all its
  // datastructures while the function is still going through them.
  //
  // TODO(crbug.com/806868): make sure 'all_done' callback is called
  // asynchronously and fix unit tests accordingly.
  CheckDone();
}

void BatchElementChecker::OnElementChecked(
    std::vector<ElementCheckCallback>* callbacks,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(element_status, *element_result);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::OnFieldValueChecked(
    std::vector<GetFieldValueCallback>* callbacks,
    const ClientStatus& status,
    const std::string& value) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(status, value);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::CheckDone() {
  pending_checks_count_--;
  DCHECK_GE(pending_checks_count_, 0);
  if (pending_checks_count_ <= 0) {
    CheckElementConditions();
    std::vector<base::OnceCallback<void()>> all_done = std::move(all_done_);
    // Callbacks in all_done_ can delete the current instance. Nothing can
    // safely access |this| after this point.
    for (auto& callback : all_done) {
      std::move(callback).Run();
    }
  }
}

void BatchElementChecker::CheckElementConditions() {
  for (auto& check : element_condition_checks_) {
    std::vector<std::string> payloads;
    size_t index = 0;
    const auto match = EvaluateElementPrecondition(check.proto, check.results,
                                                   &index, &payloads);
    std::move(check.callback)
        .Run(match ? ClientStatus(ACTION_APPLIED)
                   : ClientStatus(ELEMENT_RESOLUTION_FAILED),
             payloads, check.elements);
  }
}

bool BatchElementChecker::EvaluateElementPrecondition(
    const ElementConditionProto& proto,
    const std::vector<Result>& results,
    size_t* next_result_index,
    std::vector<std::string>* payloads) {
  bool match = false;
  switch (proto.type_case()) {
    case ElementConditionProto::kAllOf: {
      match = true;
      for (const ElementConditionProto& condition :
           proto.all_of().conditions()) {
        if (!EvaluateElementPrecondition(condition, results, next_result_index,
                                         payloads)) {
          match = false;
        }
      }
      break;
    }
    case ElementConditionProto::kAnyOf: {
      for (const ElementConditionProto& condition :
           proto.any_of().conditions()) {
        if (EvaluateElementPrecondition(condition, results, next_result_index,
                                        payloads)) {
          match = true;
        }
      }
      break;
    }

    case ElementConditionProto::kNoneOf: {
      match = true;
      for (const ElementConditionProto& condition :
           proto.none_of().conditions()) {
        if (EvaluateElementPrecondition(condition, results, next_result_index,
                                        payloads)) {
          match = false;
        }
      }
      break;
    }

    case ElementConditionProto::kMatch: {
      if (*next_result_index >= results.size()) {
        NOTREACHED();
        break;
      }
      const Result& result = results[*next_result_index];
      DCHECK_EQ(Selector(proto.match()), result.selector);
      match = result.match;
      (*next_result_index)++;
      break;
    }

    case ElementConditionProto::TYPE_NOT_SET:
      match = true;  // An empty condition is true
      break;
  }
  if (match && !proto.payload().empty()) {
    payloads->emplace_back(proto.payload());
  }
  return match;
}

void BatchElementChecker::AddElementConditionResults(
    const ElementConditionProto& proto_,
    size_t element_condition_index) {
  switch (proto_.type_case()) {
    case ElementConditionProto::kAllOf:
      for (const ElementConditionProto& condition :
           proto_.all_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kAnyOf:
      for (const ElementConditionProto& condition :
           proto_.any_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kNoneOf:
      for (const ElementConditionProto& condition :
           proto_.none_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kMatch: {
      Result result;
      result.selector = Selector(proto_.match());
      if (proto_.has_client_id()) {
        result.client_id = proto_.client_id().identifier();
      }
      result.strict = proto_.require_unique_element();
      auto& results =
          element_condition_checks_[element_condition_index].results;
      results.emplace_back(result);

      if (result.selector.empty()) {
        // Empty selectors never match.
        result.match = false;
      } else {
        AddElementCheck(
            result.selector, result.strict,
            base::BindOnce(&BatchElementChecker::OnCheckElementExists,
                           weak_ptr_factory_.GetWeakPtr(),
                           element_condition_index, results.size() - 1));
      }
      break;
    }

    case ElementConditionProto::TYPE_NOT_SET:
      break;
  }
}

void BatchElementChecker::OnCheckElementExists(
    size_t element_condition_index,
    size_t result_index,
    const ClientStatus& element_status,
    const ElementFinder::Result& element_reference) {
  DCHECK(element_condition_index < element_condition_checks_.size());
  auto& check = element_condition_checks_[element_condition_index];
  DCHECK(result_index < check.results.size());
  Result& result = check.results[result_index];
  result.match = element_status.ok();
  // TODO(szermatt): Consider reporting element_status as an unexpected error
  // right away if it is neither success nor ELEMENT_RESOLUTION_FAILED.
  if (element_status.ok() && result.client_id.has_value()) {
    check.elements[*result.client_id] = element_reference.dom_object;
  }
}

}  // namespace autofill_assistant
