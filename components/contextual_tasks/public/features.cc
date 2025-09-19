// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);

namespace flag_descriptions {

const char kContextualTasksName[] = "Contextual Tasks";
const char kContextualTasksDescription[] =
    "Enable the contextual tasks feature.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
