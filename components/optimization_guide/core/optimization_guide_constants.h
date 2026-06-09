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


// The prefix for the folder where models are stored by the new install-wide
// model store.
// TODO: crbug.com/514743962 - This defines prediction model store paths and
// should be moved to
// components/optimization_guide/core/delivery/prediction_model_store.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[];

// The name of the model execution debug logs header.
// TODO: crbug.com/514743962 - Move this to the only file that uses it:
// components/optimization_guide/core/model_execution/model_execution_fetcher_impl.cc
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideModelExecutionDebugLogsHeaderKey[];

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

// Files expected to be in the text safety model bundle.
// TODO: crbug.com/514743962 - Move these to the only file that uses them:
// components/optimization_guide/core/model_execution/safety_model_info.cc
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kTsDataFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kTsSpModelFile[];

// Files expected to be in the on device model adaptation bundle.
// TODO: crbug.com/514743962 - Move this to the only file that uses it:
// components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.cc
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOnDeviceModelAdaptationWeightsFile[];

// Minimum VRAM required for audio input support (6GB).
// TODO: crbug.com/514743962 - Move this to the only file that uses it:
// components/optimization_guide/core/model_execution/performance_class.cc
inline constexpr int kOnDeviceModelAudioVramMinMb = 6144;

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
