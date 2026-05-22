// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_TASK_ID_H_
#define COMPONENTS_ACTOR_CORE_TASK_ID_H_

#include "base/types/id_type.h"

namespace actor {

using TaskId = base::IdType32<class TaskIdTag>;

static_assert(TaskId(0).is_null(), "0 must be a null TaskId");

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_TASK_ID_H_
