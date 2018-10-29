// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {
namespace {
// Waiting period between two checks.
static constexpr base::TimeDelta kCheckPeriod =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

BatchElementChecker::BatchElementChecker(WebController* web_controller)
    : web_controller_(web_controller),
      pending_checks_count_(0),
      all_found_(false),
      stopped_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_controller);
}

BatchElementChecker::~BatchElementChecker() {}

void BatchElementChecker::AddElementCheck(
    ElementCheckType check_type,
    const std::vector<std::string>& selectors,
    ElementCheckCallback callback) {
  DCHECK(!try_done_callback_);

  element_check_callbacks_[std::make_pair(check_type, selectors)].emplace_back(
      std::move(callback));
}

void BatchElementChecker::AddFieldValueCheck(
    const std::vector<std::string>& selectors,
    GetFieldValueCallback callback) {
  DCHECK(!try_done_callback_);

  get_field_value_callbacks_[selectors].emplace_back(std::move(callback));
}

void BatchElementChecker::Run(const base::TimeDelta& duration,
                              base::RepeatingCallback<void()> try_done,
                              base::OnceCallback<void()> all_done) {
  int64_t try_count = duration / kCheckPeriod;
  if (try_count <= 0) {
    try_count = 1;
  }

  Try(base::BindOnce(
      &BatchElementChecker::OnTryDone,
      // Callback is run from this class, so this is guaranteed to still exist.
      base::Unretained(this), try_count, try_done, std::move(all_done)));
}

void BatchElementChecker::Try(base::OnceCallback<void()> try_done_callback) {
  DCHECK(!try_done_callback_);

  if (stopped_) {
    std::move(try_done_callback).Run();
    return;
  }
  try_done_callback_ = std::move(try_done_callback);

  DCHECK_EQ(pending_checks_count_, 0);
  pending_checks_count_ =
      element_check_callbacks_.size() + get_field_value_callbacks_.size() + 1;

  for (auto& entry : element_check_callbacks_) {
    if (entry.second.empty()) {
      pending_checks_count_--;
      continue;
    }

    const auto& call_arguments = entry.first;
    web_controller_->ElementCheck(
        call_arguments.first, call_arguments.second,
        base::BindOnce(
            &BatchElementChecker::OnElementChecked,
            weak_ptr_factory_.GetWeakPtr(),
            // Guaranteed to exist for the lifetime of this instance, because
            // the map isn't modified after Run has been called.
            base::Unretained(&entry.second)));
  }

  for (auto& entry : get_field_value_callbacks_) {
    if (entry.second.empty()) {
      pending_checks_count_--;
      continue;
    }

    web_controller_->GetFieldValue(
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
  pending_checks_count_--;
  CheckTryDone();
}

void BatchElementChecker::OnTryDone(int64_t remaining_attempts,
                                    base::RepeatingCallback<void()> try_done,
                                    base::OnceCallback<void()> all_done) {
  if (all_found_) {
    try_done.Run();
    std::move(all_done).Run();
    return;
  }

  --remaining_attempts;
  if (remaining_attempts <= 0 || stopped_) {
    // GiveUp is run before calling try_done, so its effects are visible right
    // away.
    GiveUp();
    try_done.Run();
    std::move(all_done).Run();
    return;
  }
  try_done.Run();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &BatchElementChecker::Try, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&BatchElementChecker::OnTryDone,
                         weak_ptr_factory_.GetWeakPtr(), remaining_attempts,
                         try_done, std::move(all_done))),
      kCheckPeriod);
}

void BatchElementChecker::GiveUp() {
  for (auto& entry : element_check_callbacks_) {
    RunCallbacks(&entry.second, false);
  }
  for (auto& entry : get_field_value_callbacks_) {
    RunCallbacks(&entry.second, false, "");
  }
}

void BatchElementChecker::OnElementChecked(
    std::vector<ElementCheckCallback>* callbacks,
    bool exists) {
  pending_checks_count_--;
  if (exists)
    RunCallbacks(callbacks, true);

  CheckTryDone();
}

void BatchElementChecker::OnGetFieldValue(
    std::vector<GetFieldValueCallback>* callbacks,
    bool exists,
    const std::string& value) {
  pending_checks_count_--;
  if (exists)
    RunCallbacks(callbacks, exists, value);

  CheckTryDone();
}

void BatchElementChecker::CheckTryDone() {
  DCHECK_GE(pending_checks_count_, 0);
  if (pending_checks_count_ <= 0 && try_done_callback_) {
    all_found_ = !HasMoreChecksToRun();
    std::move(try_done_callback_).Run();
  }
}

void BatchElementChecker::RunCallbacks(
    std::vector<ElementCheckCallback>* callbacks,
    bool result) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(result);
  }
  callbacks->clear();
}

void BatchElementChecker::RunCallbacks(
    std::vector<GetFieldValueCallback>* callbacks,
    bool exists,
    const std::string& value) {
  for (auto& callback : *callbacks) {
    std::move(callback).Run(exists, value);
  }
  callbacks->clear();
}

bool BatchElementChecker::HasMoreChecksToRun() {
  for (const auto& entry : element_check_callbacks_) {
    if (!entry.second.empty())
      return true;
  }
  for (const auto& entry : get_field_value_callbacks_) {
    if (!entry.second.empty())
      return true;
  }
  return false;
}
}  // namespace autofill_assistant
