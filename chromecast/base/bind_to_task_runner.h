// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
#define CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

// Helpers for using base::BindPostTask() with the TaskRunner for the current
// sequence, ie. base::SequencedTaskRunner::GetCurrentDefault(), or current
// thread, ie, base::SingleThreadTaskRunner::GetCurrentDefault().
namespace chromecast {

template <typename T>
base::OnceCallback<T> BindToCurrentThread(
    base::OnceCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            std::move(callback), location);
}

template <typename T>
base::RepeatingCallback<T> BindToCurrentThread(
    base::RepeatingCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            std::move(callback), location);
}

template <typename T>
base::OnceCallback<T> BindToCurrentSequence(
    base::OnceCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTaskToCurrentDefault(std::move(callback), location);
}

template <typename T>
base::RepeatingCallback<T> BindToCurrentSequence(
    base::RepeatingCallback<T> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTaskToCurrentDefault(std::move(callback), location);
}

}  // namespace chromecast

#endif  // CHROMECAST_BASE_BIND_TO_TASK_RUNNER_H_
