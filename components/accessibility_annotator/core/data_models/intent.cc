// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/data_models/intent.h"

namespace accessibility_annotator {

TaskIntent::TaskIntent() = default;
TaskIntent::TaskIntent(const TaskIntent& other) = default;
TaskIntent::TaskIntent(TaskIntent&& other) = default;
TaskIntent& TaskIntent::operator=(const TaskIntent& other) = default;
TaskIntent& TaskIntent::operator=(TaskIntent&& other) = default;
TaskIntent::~TaskIntent() = default;

}  // namespace accessibility_annotator
