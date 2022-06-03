// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_

#include "base/task/task_traits.h"

namespace update_client {

// Task traits for tasks posted to base::ThreadPool from update_client.

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

constexpr base::TaskTraits kTaskTraitsBackgroundDownloader = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

// This task joins a process, hence .WithBaseSyncPrimitives().
constexpr base::TaskTraits kTaskTraitsRunCommand = {
    base::MayBlock(), base::WithBaseSyncPrimitives(),
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_
