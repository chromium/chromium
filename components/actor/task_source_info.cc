// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/task_source_info.h"

namespace actor {

TaskSourceInfo::TaskSourceInfo(Client type, std::optional<SourceDefinedId> id)
    : type(type), id(std::move(id)) {}

TaskSourceInfo::~TaskSourceInfo() = default;

TaskSourceInfo::TaskSourceInfo(const TaskSourceInfo&) = default;

TaskSourceInfo& TaskSourceInfo::operator=(const TaskSourceInfo&) = default;

TaskSourceInfo::TaskSourceInfo(TaskSourceInfo&&) = default;

TaskSourceInfo& TaskSourceInfo::operator=(TaskSourceInfo&&) = default;

}  // namespace actor
