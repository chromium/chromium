// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these constants should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace optimization_guide {

// Files expected to be in the on device model bundle.
// TODO: crbug.com/514743962 - Move these to
// components/optimization_guide/core/model_execution/on_device_model_component.h
// or
// components/optimization_guide/core/model_execution/on_device_model_metadata.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kWeightsFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kWeightCacheFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kEncoderCacheFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kAdapterCacheFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kProgramCacheFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOnDeviceModelExecutionConfigFile[];

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
