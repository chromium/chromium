// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_BIND_TO_TASK_RUNNER_H_
#define COMPONENTS_SYNC_BASE_BIND_TO_TASK_RUNNER_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

// This is a helper utility for Bind()ing callbacks to a given TaskRunner.
// The typical use is when |a| (of class |A|) wants to hand a callback such as
// base::BindOnce(&A::AMethod, a) to |b|, but needs to ensure that when |b|
// executes the callback, it does so on a specific TaskRunner (for example,
// |a|'s current MessageLoop).
//
// Typical usage: request to be called back on the current sequence:
// other->StartAsyncProcessAndCallMeBack(BindToTaskRunner(
//     my_task_runner_, base::BindOnce(&MyClass::MyMethod, this)));
//
// Note that like base::Bind{Once,Repeating}(), BindToTaskRunner() can't bind
// non-constant references, and that *unlike* base::Bind{Once,Repeating}(),
// BindToTaskRunner() makes copies of its arguments, and thus can't be used with
// arrays. Note that the callback is always posted to the target TaskRunner.
//
// As a convenience, you can use BindToCurrentSequence() to bind to the
// TaskRunner for the current sequence (i.e.
// base::SequencedTaskRunnerHandle::Get()).

namespace syncer {
namespace bind_helpers {

template <typename Sig>
struct BindToTaskRunnerTrampoline;

template <typename... Args>
struct BindToTaskRunnerTrampoline<void(Args...)> {
  static void Run(const scoped_refptr<base::TaskRunner>& task_runner,
                  base::OnceCallback<void(Args...)> cb,
                  Args... args) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::forward<Args>(args)...));
  }
};

}  // namespace bind_helpers

template <typename T>
base::OnceCallback<T> BindToTaskRunner(
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::OnceCallback<T> cb) {
  return base::BindOnce(&bind_helpers::BindToTaskRunnerTrampoline<T>::Run,
                        task_runner, std::move(cb));
}

template <typename T>
base::RepeatingCallback<T> BindToTaskRunner(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::RepeatingCallback<T>& cb) {
  return base::BindRepeating(&bind_helpers::BindToTaskRunnerTrampoline<T>::Run,
                             task_runner, cb);
}

template <typename T>
base::OnceCallback<T> BindToCurrentSequence(base::OnceCallback<T> cb) {
  return BindToTaskRunner(base::SequencedTaskRunnerHandle::Get(),
                          std::move(cb));
}

template <typename T>
base::RepeatingCallback<T> BindToCurrentSequence(
    const base::RepeatingCallback<T>& cb) {
  return BindToTaskRunner(base::SequencedTaskRunnerHandle::Get(), cb);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_BIND_TO_TASK_RUNNER_H_
