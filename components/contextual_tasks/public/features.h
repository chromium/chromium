// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace contextual_tasks {

BASE_DECLARE_FEATURE(kContextualTasks);

namespace flag_descriptions {

extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
