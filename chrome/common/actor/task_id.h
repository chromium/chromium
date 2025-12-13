// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_TASK_ID_H_
#define CHROME_COMMON_ACTOR_TASK_ID_H_

#include "base/types/id_type.h"

namespace actor {

using TaskId = base::IdType32<class TaskIdTag>;

static_assert(TaskId(0).is_null(), "0 must be a null TaskId");

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_TASK_ID_H_
