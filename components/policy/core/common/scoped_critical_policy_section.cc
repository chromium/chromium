// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/scoped_critical_policy_section.h"

#include <windows.h>

#include <userenv.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

namespace {

void EnterSection(
    ScopedCriticalPolicySection::OnSectionEnteredCallback callback) {
  ScopedCriticalPolicySection::Handles handles;
  // We need both user and machine handles. Based on MSFT doc, user handle must
  // be acquired first to prevent dead lock.
  // https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-entercriticalpolicysection
  //
  // If we failed to aquire lock or the API is timeout, we will read the policy
  // regardless, as we used to have.
  handles.user_handle = ::EnterCriticalPolicySection(false);
  if (!handles.user_handle) {
    PLOG(WARNING) << "Failed to enter user critical policy section.";
  }
  handles.machine_handle = ::EnterCriticalPolicySection(true);
  if (!handles.machine_handle) {
    PLOG(WARNING) << "Failed to enter machine critical policy section.";
  }
  std::move(callback).Run(handles);
}

}  // namespace

// static
void ScopedCriticalPolicySection::Enter(
    base::OnceClosure callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  EnterWithEnterSectionCallback(std::move(callback), EnterSectionCallback(),
                                task_runner);
}

// static
void ScopedCriticalPolicySection::EnterWithEnterSectionCallback(
    base::OnceClosure callback,
    EnterSectionCallback enter_section_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  auto scoped_section =
      std::make_unique<ScopedCriticalPolicySection>(task_runner);

  scoped_section->enter_section_callback_ =
      enter_section_callback ? std::move(enter_section_callback)
                             : base::BindOnce(&EnterSection);
  scoped_section->Init(base::BindOnce(
      [](std::unique_ptr<ScopedCriticalPolicySection> scoped_section,
         base::OnceClosure callback) { std::move(callback).Run(); },
      std::move(scoped_section), std::move(callback)));
}

ScopedCriticalPolicySection::ScopedCriticalPolicySection(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

ScopedCriticalPolicySection::~ScopedCriticalPolicySection() {
  if (machine_handle_) {
    ::LeaveCriticalPolicySection(machine_handle_);
  }

  if (user_handle_) {
    ::LeaveCriticalPolicySection(user_handle_);
  }
}

void ScopedCriticalPolicySection::Init(base::OnceClosure callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (enter_section_callback_) {
    // Call ::EnterCriticalPolicySection in a different thread as the API could
    // take minutes to return.
    // Using `PostTask` instead of `PostTaskAndReplyWithResult` allows unit test
    // mimic blocking function easily.
    auto on_section_entered = base::BindPostTask(
        task_runner_,
        base::BindOnce(&ScopedCriticalPolicySection::OnSectionEntered,
                       weak_factory_.GetWeakPtr()));
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(std::move(enter_section_callback_),
                       std::move(on_section_entered)));
  }

  // Based on UMA data, 15 seconds timeout is enough for 99.9% cases.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScopedCriticalPolicySection::OnSectionEntered,
                     weak_factory_.GetWeakPtr(), Handles()),
      base::Seconds(15));
}

void ScopedCriticalPolicySection::OnSectionEntered(Handles handles) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  machine_handle_ = handles.machine_handle;
  user_handle_ = handles.user_handle;
  std::move(callback_).Run();
}

}  // namespace policy
