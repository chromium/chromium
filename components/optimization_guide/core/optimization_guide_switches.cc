// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/command_line.h"

namespace optimization_guide {
namespace switches {

// Overrides the on-device model file paths for on-device model execution.
const char kOnDeviceModelExecutionOverride[] =
    "optimization-guide-ondevice-model-execution-override";

std::optional<base::FilePath> GetOnDeviceModelExecutionOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceModelExecutionOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(kOnDeviceModelExecutionOverride);
}

}  // namespace switches
}  // namespace optimization_guide
