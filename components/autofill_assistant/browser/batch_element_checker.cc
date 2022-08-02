// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/selector_observer.h"
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

void BatchElementChecker::AddElementConditionCheck(
    const ElementConditionProto& condition,
    ElementConditionCheckCallback callback) {
  DCHECK(!started_);
  if (IsElementConditionEmpty(condition)) {
    std::move(callback).Run(ClientStatus(ACTION_APPLIED), {}, {}, {});
    return;
  }

  BatchElementChecker::ElementConditionCheck check;
  check.proto = condition;
  check.callback = std::move(callback);
  element_condition_checks_.emplace_back(std::move(check));
}

// TODO(b/215335501): Refactor out of BatchElementChecker.
void BatchElementChecker::AddFieldValueCheck(const Selector& selector,
                                             GetFieldValueCallback callback) {
  DCHECK(!started_);

  get_field_value_callbacks_[selector].emplace_back(std::move(callback));
}

bool BatchElementChecker::empty() const {
  return element_condition_checks_.empty() &&
         get_field_value_callbacks_.empty();
}

bool BatchElementChecker::IsElementConditionEmpty(
    const ElementConditionProto& proto) {
  return proto.type_case() == ElementConditionProto::TYPE_NOT_SET;
}

void BatchElementChecker::AddAllDoneCallback(
    base::OnceCallback<void()> all_done) {
  all_done_.emplace_back(std::move(all_done));
}

void BatchElementChecker::EnableObserver(
    const SelectorObserver::Settings& settings) {
  DCHECK(!observer_settings_);
  DCHECK(!started_);
  DCHECK(get_field_value_callbacks_.empty())
      << "Observer-based BatchElementChecker doesn't work with "
         "AddFieldValueCheck";
  observer_settings_.emplace(settings);
}

void BatchElementChecker::Run(WebController* web_controller) {
  DCHECK(web_controller);
  DCHECK(!started_);
  for (size_t i = 0; i < element_condition_checks_.size(); ++i) {
    AddElementConditionResults(element_condition_checks_[i].proto, i);
  }
  if (observer_settings_) {
    RunWithObserver(web_controller);
    return;
  }
  started_ = true;

  pending_checks_count_ =
      unique_selectors_.size() + get_field_value_callbacks_.size() + 1;

  for (auto& entry : unique_selectors_) {
    web_controller->FindElement(
        /* selector= */ entry.first.first,
        /* strict= */ entry.first.second,
        base::BindOnce(
            &BatchElementChecker::OnSelectorChecked,
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

void BatchElementChecker::RunWithObserver(WebController* web_controller) {
  DCHECK(get_field_value_callbacks_.empty())
      << "Observer-based BatchElementChecker doesn't work with "
         "AddFieldValueCheck";
  DCHECK(observer_settings_);
  DCHECK(!started_);
  std::vector<SelectorObserver::ObservableSelector> selectors;

  size_t index = 0;
  for (auto& entry : unique_selectors_) {
    selectors.emplace_back(SelectorObserver::SelectorId(index++),
                           /* proto = */ entry.first.first.proto,
                           /* strict = */ entry.first.second);
  }
  if (selectors.size() == 0) {
    FinishedCallbacks();
    return;
  }
  started_ = true;
  auto result = web_controller->ObserveSelectors(
      selectors, *observer_settings_,
      base::BindRepeating(&BatchElementChecker::OnResultsUpdated,
                          weak_ptr_factory_.GetWeakPtr())

  );
  if (!result.ok()) {
    CallAllCallbacksWithError(result);
  }
}

void BatchElementChecker::OnResultsUpdated(
    const ClientStatus& status,
    const std::vector<SelectorObserver::Update>& updates,
    SelectorObserver* selector_observer) {
  bool continue_checking = true;
  if (!status.ok()) {
    if (status.proto_status() == ELEMENT_RESOLUTION_FAILED) {
      continue_checking = false;
    } else {
      CallAllCallbacksWithError(status);
      return;
    }
  }
  std::vector<size_t> updated_conditions_vector;
  // Apply updates
  for (auto& update : updates) {
    size_t selector_id = update.selector_id.value();
    DCHECK_LT(selector_id, unique_selectors_.size());
    auto& affected_results =
        std::next(unique_selectors_.begin(), selector_id)->second;
    for (auto& pair : affected_results) {
      size_t condition_index = pair.first;
      size_t result_index = pair.second;
      auto& condition = element_condition_checks_[condition_index];
      auto& result = condition.results[result_index];
      result.match = {/* has_value= */ true, /* value= */ update.match};
      result.element_id = update.match ? update.element_id : -1;
      updated_conditions_vector.emplace_back(condition_index);
    }
  }
  auto updated_conditions =
      base::flat_set<size_t>(std::move(updated_conditions_vector));

  for (size_t condition_index : updated_conditions) {
    auto& condition = element_condition_checks_[condition_index];
    size_t index = 0;
    if (EvaluateElementPrecondition(condition.proto, condition.results, &index,
                                    nullptr, nullptr)
            .matches()) {
      continue_checking = false;
      break;
    }
  }
  if (continue_checking) {
    selector_observer->Continue();
    return;
  }
  std::vector<SelectorObserver::RequestedElement> wanted_elements;
  size_t index = 0;
  for (auto& entry : unique_selectors_) {
    for (auto& pair : entry.second) {
      size_t condition_index = pair.first;
      size_t result_index = pair.second;
      auto& condition = element_condition_checks_[condition_index];
      auto& result = condition.results[result_index];
      if (result.match.matches() && result.client_id) {
        wanted_elements.emplace_back(SelectorObserver::SelectorId(index),
                                     result.element_id);
      }
    }
    ++index;
  }
  selector_observer->GetElementsAndStop(
      wanted_elements, base::BindOnce(&BatchElementChecker::OnGetElementsDone,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void BatchElementChecker::OnGetElementsDone(
    const ClientStatus& status,
    const base::flat_map<SelectorObserver::SelectorId, DomObjectFrameStack>&
        elements) {
  if (!status.ok()) {
    CallAllCallbacksWithError(status);
    return;
  }
  for (auto& element : elements) {
    size_t results_index = element.first.value();
    DCHECK_LT(results_index, unique_selectors_.size());
    auto& affected_results =
        std::next(unique_selectors_.begin(), results_index)->second;
    for (auto& pair : affected_results) {
      size_t condition_index = pair.first;
      size_t result_index = pair.second;
      DCHECK(condition_index < element_condition_checks_.size());
      auto& condition = element_condition_checks_[condition_index];
      DCHECK(result_index < condition.results.size());
      auto& result = condition.results[result_index];
      DCHECK(result.client_id.has_value());
      condition.elements[result.client_id.value()] = element.second;
    }
  }
  CheckElementConditions();
  FinishedCallbacks();
}

void BatchElementChecker::CallAllCallbacksWithError(
    const ClientStatus& status) {
  DCHECK(!status.ok());
  for (auto& entry : element_condition_checks_) {
    std::move(entry.callback).Run(status, {}, {}, {});
  }
  FinishedCallbacks();
}

void BatchElementChecker::OnSelectorChecked(
    std::vector<std::pair</* element_condition_index */ size_t,
                          /* result_index */ size_t>>* results,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinderResult> element_result) {
  for (auto& pair : *results) {
    size_t condition_index = pair.first;
    size_t result_index = pair.second;
    DCHECK(condition_index < element_condition_checks_.size());
    auto& condition = element_condition_checks_[condition_index];
    DCHECK(result_index < condition.results.size());
    Result& result = condition.results[result_index];
    result.match = {/* has_value= */ true, /* value= */ element_status.ok()};
    // TODO(szermatt): Consider reporting element_status as an unexpected error
    // right away if it is neither success nor ELEMENT_RESOLUTION_FAILED.
    if (element_status.ok() && result.client_id.has_value()) {
      condition.elements[*result.client_id] = element_result->dom_object();
    }
  }
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
    FinishedCallbacks();
  }
}

void BatchElementChecker::FinishedCallbacks() {
  std::vector<base::OnceCallback<void()>> all_done = std::move(all_done_);
  // Callbacks in all_done_ can delete the current instance. Nothing can
  // safely access |this| after this point.
  for (auto& callback : all_done) {
    std::move(callback).Run();
  }
}

void BatchElementChecker::CheckElementConditions() {
  for (auto& check : element_condition_checks_) {
    std::vector<std::string> payloads;
    std::vector<std::string> tags;
    size_t index = 0;
    bool match = EvaluateElementPrecondition(check.proto, check.results, &index,
                                             &payloads, &tags)
                     .matches();
    std::move(check.callback)
        .Run(match ? ClientStatus(ACTION_APPLIED)
                   : ClientStatus(ELEMENT_RESOLUTION_FAILED),
             payloads, tags, check.elements);
  }
}

BatchElementChecker::MatchResult
BatchElementChecker::EvaluateElementPrecondition(
    const ElementConditionProto& proto,
    const std::vector<Result>& results,
    size_t* next_result_index,
    std::vector<std::string>* payloads,
    std::vector<std::string>* tags) {
  MatchResult match{/* checked = */ true, /* match_result= */ false};
  switch (proto.type_case()) {
    case ElementConditionProto::kAllOf: {
      match.match_result = true;
      for (const ElementConditionProto& condition :
           proto.all_of().conditions()) {
        MatchResult result = EvaluateElementPrecondition(
            condition, results, next_result_index, payloads, tags);
        if (match.checked && result.checked) {
          match.match_result = match.match_result && result.match_result;
        } else {
          match.checked = false;
        }
      }
      break;
    }
    case ElementConditionProto::kAnyOf: {
      for (const ElementConditionProto& condition :
           proto.any_of().conditions()) {
        if (EvaluateElementPrecondition(condition, results, next_result_index,
                                        payloads, tags)
                .matches()) {
          match.match_result = true;
        }
      }
      break;
    }

    case ElementConditionProto::kNoneOf: {
      match.match_result = true;
      for (const ElementConditionProto& condition :
           proto.none_of().conditions()) {
        MatchResult result = EvaluateElementPrecondition(
            condition, results, next_result_index, payloads, tags);
        if (match.checked && result.checked) {
          match.match_result = match.match_result && !result.match_result;
        } else {
          match.checked = false;
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
      match.match_result = true;  // An empty condition is true
      break;
  }
  if (payloads && match.matches() && !proto.payload().empty()) {
    payloads->emplace_back(proto.payload());
  }
  if (tags && match.matches() && !proto.tag().empty()) {
    tags->emplace_back(proto.tag());
  }
  return match;
}

void BatchElementChecker::AddElementConditionResults(
    const ElementConditionProto& proto,
    size_t element_condition_index) {
  switch (proto.type_case()) {
    case ElementConditionProto::kAllOf:
      for (const ElementConditionProto& condition :
           proto.all_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kAnyOf:
      for (const ElementConditionProto& condition :
           proto.any_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kNoneOf:
      for (const ElementConditionProto& condition :
           proto.none_of().conditions()) {
        AddElementConditionResults(condition, element_condition_index);
      }
      break;

    case ElementConditionProto::kMatch: {
      auto& results =
          element_condition_checks_[element_condition_index].results;
      Result& result = results.emplace_back();
      result.selector = Selector(proto.match());
      if (proto.has_client_id()) {
        result.client_id = proto.client_id().identifier();
      }
      result.strict = proto.require_unique_element();
      if (result.selector.empty()) {
        // Empty selectors never match.
        VLOG(1) << __func__
                << " Received invalid selector: " << result.selector;
        result.match = {/* has_value= */ true, /* value= */ false};
      } else {
        unique_selectors_[std::make_pair(result.selector, result.strict)]
            .emplace_back(element_condition_index, results.size() - 1);
      }
      break;
    }

    case ElementConditionProto::TYPE_NOT_SET:
      break;
  }
}
}  // namespace autofill_assistant
