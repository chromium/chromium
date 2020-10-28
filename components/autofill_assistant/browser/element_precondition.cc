// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_precondition.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"

namespace autofill_assistant {

ElementPrecondition::ElementPrecondition(const ElementConditionProto& proto)
    : proto_(proto) {
  AddResults(proto_);
}

ElementPrecondition::~ElementPrecondition() = default;

void ElementPrecondition::Check(
    BatchElementChecker* batch_checks,
    base::OnceCallback<void(const ClientStatus&,
                            const std::vector<std::string>&)> callback) {
  if (results_.empty()) {
    OnAllElementChecksDone(std::move(callback));
    return;
  }

  for (size_t i = 0; i < results_.size(); i++) {
    Result& result = results_[i];
    result.match = false;
    if (result.selector.empty()) {
      // Empty selectors never match.
      continue;
    }
    batch_checks->AddElementCheck(
        result.selector,
        base::BindOnce(&ElementPrecondition::OnCheckElementExists,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
  batch_checks->AddAllDoneCallback(
      base::BindOnce(&ElementPrecondition::OnAllElementChecksDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ElementPrecondition::OnCheckElementExists(
    size_t result_index,
    const ClientStatus& element_status,
    const ElementFinder::Result& element_reference) {
  if (result_index >= results_.size()) {
    NOTREACHED();
    return;
  }
  Result& result = results_[result_index];
  result.match = element_status.ok();
  // TODO(szermatt): Consider reporting element_status as an unexpected error
  // right away if it is neither success nor ELEMENT_RESOLUTION_FAILED.
  // TODO(b/171782156): Store the element, if the status is ok.
}

void ElementPrecondition::OnAllElementChecksDone(
    base::OnceCallback<void(const ClientStatus& status,
                            const std::vector<std::string>& payloads)>
        callback) {
  size_t next_result_index = 0;
  std::vector<std::string> payloads;
  bool match = EvaluateResults(proto_, &next_result_index, &payloads);
  DCHECK_EQ(next_result_index, results_.size());
  std::move(callback).Run(match ? ClientStatus(ACTION_APPLIED)
                                : ClientStatus(ELEMENT_RESOLUTION_FAILED),
                          payloads);
}

bool ElementPrecondition::EvaluateResults(const ElementConditionProto& proto_,
                                          size_t* next_result_index,
                                          std::vector<std::string>* payloads) {
  bool match = false;
  switch (proto_.type_case()) {
    case ElementConditionProto::kAllOf: {
      match = true;
      for (const ElementConditionProto& condition :
           proto_.all_of().conditions()) {
        if (!EvaluateResults(condition, next_result_index, payloads)) {
          match = false;
        }
      }
      break;
    }
    case ElementConditionProto::kAnyOf: {
      for (const ElementConditionProto& condition :
           proto_.any_of().conditions()) {
        if (EvaluateResults(condition, next_result_index, payloads)) {
          match = true;
        }
      }
      break;
    }

    case ElementConditionProto::kNoneOf: {
      match = true;
      for (const ElementConditionProto& condition :
           proto_.none_of().conditions()) {
        if (EvaluateResults(condition, next_result_index, payloads)) {
          match = false;
        }
      }
      break;
    }

    case ElementConditionProto::kMatch: {
      if (*next_result_index >= results_.size()) {
        NOTREACHED();
        break;
      }
      const Result& result = results_[*next_result_index];
      DCHECK_EQ(Selector(proto_.match()), result.selector);
      match = result.match;
      (*next_result_index)++;
      break;
    }

    case ElementConditionProto::TYPE_NOT_SET:
      match = true;  // An empty condition is true
      break;
  }
  if (match && !proto_.payload().empty()) {
    payloads->emplace_back(proto_.payload());
  }
  return match;
}

void ElementPrecondition::AddResults(const ElementConditionProto& proto_) {
  switch (proto_.type_case()) {
    case ElementConditionProto::kAllOf:
      for (const ElementConditionProto& condition :
           proto_.all_of().conditions()) {
        AddResults(condition);
      }
      break;

    case ElementConditionProto::kAnyOf:
      for (const ElementConditionProto& condition :
           proto_.any_of().conditions()) {
        AddResults(condition);
      }
      break;

    case ElementConditionProto::kNoneOf:
      for (const ElementConditionProto& condition :
           proto_.none_of().conditions()) {
        AddResults(condition);
      }
      break;

    case ElementConditionProto::kMatch: {
      Result result;
      result.selector = Selector(proto_.match());
      results_.emplace_back(result);
      break;
    }

    case ElementConditionProto::TYPE_NOT_SET:
      break;
  }
}

}  // namespace autofill_assistant
