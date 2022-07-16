// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
#define CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"

// Helpers for using base::BindPostTask() with the TaskRunner for the current
// sequence, ie. base::SequencedTaskRunnerHandle::Get(), or current thread, ie,
// base::ThreadTaskRunnerHandle::Get().
namespace chromecast {

template <typename T>
base::OnceCallback<T> BindToCurrentThread(
    base::OnceCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                            std::move(callback), location);
}

template <typename T>
base::RepeatingCallback<T> BindToCurrentThread(
    base::RepeatingCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                            std::move(callback), location);
}

template <typename T>
base::OnceCallback<T> BindToCurrentSequence(
    base::OnceCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(callback), location);
}

template <typename T>
base::RepeatingCallback<T> BindToCurrentSequence(
    base::RepeatingCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(callback), location);
}

}  // namespace chromecast

#endif  // CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
