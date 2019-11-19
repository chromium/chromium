// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

BatchElementChecker::BatchElementChecker() {}

BatchElementChecker::~BatchElementChecker() {}

void BatchElementChecker::AddElementCheck(const Selector& selector,
                                          ElementCheckCallback callback) {
  DCHECK(!started_);

  element_check_callbacks_[selector].emplace_back(std::move(callback));
}

void BatchElementChecker::AddFieldValueCheck(const Selector& selector,
                                             GetFieldValueCallback callback) {
  DCHECK(!started_);

  get_field_value_callbacks_[selector].emplace_back(std::move(callback));
}

bool BatchElementChecker::empty() const {
  return element_check_callbacks_.empty() && get_field_value_callbacks_.empty();
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
    web_controller->ElementCheck(
        entry.first, /* strict= */ false,
        base::BindOnce(
            &BatchElementChecker::OnElementChecked,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
  }

  for (auto& entry : get_field_value_callbacks_) {
    web_controller->GetFieldValue(
        entry.first,
        base::BindOnce(
            &BatchElementChecker::OnGetFieldValue,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
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
    const ClientStatus& element_status) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(element_status);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::OnGetFieldValue(
    std::vector<GetFieldValueCallback>* callbacks,
    const ClientStatus& element_status,
    const std::string& value) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(element_status, value);
  }
  callbacks->clear();
  CheckDone();
}

void BatchElementChecker::CheckDone() {
  pending_checks_count_--;
  DCHECK_GE(pending_checks_count_, 0);
  if (pending_checks_count_ <= 0) {
    std::vector<base::OnceCallback<void()>> all_done = std::move(all_done_);
    // Callbacks in all_done_ can delete the current instance. Nothing can
    // safely access |this| after this point.
    for (auto& callback : all_done) {
      std::move(callback).Run();
    }
  }
}

}  // namespace autofill_assistant
