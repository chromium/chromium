// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_FLING_SCHEDULER_BASE_H_
#define COMPONENTS_INPUT_FLING_SCHEDULER_BASE_H_

#include "components/input/fling_controller.h"

namespace input {

class FlingSchedulerBase : public FlingControllerSchedulerClient {
 public:
  virtual void ProgressFlingOnBeginFrameIfneeded(
      base::TimeTicks current_time) = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_FLING_SCHEDULER_BASE_H_
