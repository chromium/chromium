// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_precondition.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {

ElementPrecondition::ElementPrecondition(
    const google::protobuf::RepeatedPtrField<ElementReferenceProto>&
        element_exists,
    const google::protobuf::RepeatedPtrField<FormValueMatchProto>&
        form_value_match)
    : form_value_match_(form_value_match.begin(), form_value_match.end()) {
  for (const auto& element : element_exists) {
    // TODO(crbug.com/806868): Check if we shouldn't skip the script when this
    // happens.
    if (element.selectors_size() == 0) {
      DVLOG(3) << "Ignored empty selectors in script precondition.";
      continue;
    }

    elements_exist_.emplace_back(Selector(element));
  }
}

ElementPrecondition::~ElementPrecondition() = default;

void ElementPrecondition::Check(BatchElementChecker* batch_checks,
                                base::OnceCallback<void(bool)> callback) {
  pending_check_count_ = elements_exist_.size() + form_value_match_.size();
  if (pending_check_count_ == 0) {
    std::move(callback).Run(true);
    return;
  }

  callback_ = std::move(callback);
  for (const auto& selector : elements_exist_) {
    base::OnceCallback<void(const ClientStatus&)> callback =
        base::BindOnce(&ElementPrecondition::OnCheckElementExists,
                       weak_ptr_factory_.GetWeakPtr());
    batch_checks->AddElementCheck(selector, std::move(callback));
  }
  for (size_t i = 0; i < form_value_match_.size(); i++) {
    const auto& value_match = form_value_match_[i];
    DCHECK(!value_match.element().selectors().empty());
    batch_checks->AddFieldValueCheck(
        Selector(value_match.element()),
        base::BindOnce(&ElementPrecondition::OnGetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void ElementPrecondition::OnCheckElementExists(
    const ClientStatus& element_status) {
  ReportCheckResult(element_status.ok());
}

void ElementPrecondition::OnGetFieldValue(int index,
                                          const ClientStatus& element_status,
                                          const std::string& value) {
  if (!element_status.ok()) {
    ReportCheckResult(false);
    return;
  }

  // TODO(crbug.com/806868): Differentiate between empty value and failure.
  const auto& value_match = form_value_match_[index];
  if (value_match.has_value()) {
    ReportCheckResult(value == value_match.value());
    return;
  }

  ReportCheckResult(!value.empty());
}

void ElementPrecondition::ReportCheckResult(bool success) {
  if (!callback_)
    return;

  if (!success) {
    std::move(callback_).Run(false);
    return;
  }

  --pending_check_count_;
  if (pending_check_count_ <= 0) {
    DCHECK_EQ(pending_check_count_, 0);
    std::move(callback_).Run(true);
  }
}

}  // namespace autofill_assistant
