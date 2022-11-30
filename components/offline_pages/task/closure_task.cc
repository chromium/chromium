// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/closure_task.h"
namespace offline_pages {

ClosureTask::ClosureTask(base::OnceClosure closure)
    : closure_(std::move(closure)) {}
ClosureTask::~ClosureTask() = default;

void ClosureTask::Run() {
  std::move(closure_).Run();
  TaskComplete();
}

}  // namespace offline_pages
