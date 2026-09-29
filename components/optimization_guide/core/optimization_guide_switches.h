// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these switches should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace optimization_guide {
namespace switches {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelExecutionOverride[];
// Returns the path to the on-device base model provided on the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceModelExecutionOverride();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
