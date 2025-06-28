// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_base.h"

namespace actor {

base::TimeDelta ToolBase::ExecutionObservationDelay() const {
  return base::TimeDelta();
}

ToolBase::ToolBase(content::RenderFrame& frame,
                   Journal::TaskId task_id,
                   Journal& journal,
                   mojom::ToolTargetPtr target,
                   mojom::ObservedToolTargetPtr observed_target)
    : frame_(frame),
      task_id_(task_id),
      journal_(journal),
      target_(std::move(target)),
      observed_target_(std::move(observed_target)) {}

ToolBase::~ToolBase() = default;

}  // namespace actor
