// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_base.h"

namespace actor {

base::TimeDelta ToolBase::ExecutionObservationDelay() const {
  return base::TimeDelta();
}

}  // namespace actor
