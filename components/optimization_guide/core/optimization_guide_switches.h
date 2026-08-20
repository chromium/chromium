// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these switches should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace optimization_guide {
namespace switches {

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/model_execution_fetcher_impl.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelExecutionEnableRemoteDebugLogging[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/on_device_model_component.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelExecutionOverride[];
// Returns the path to the on-device base model provided on the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceModelExecutionOverride();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGetFreeDiskSpaceWithUserVisiblePriorityTask[];
// Returns whether to get free disk space with base::TaskPriority::USER_VISIBLE
// task. This is about the freediskspace check in the context of the on-device
// model eligibility check.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelAdaptationsOverride[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceAPIKey[];
// The API key for the ModelQualityLoggingService.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetModelQualityServiceAPIKey();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/model_execution_features_controller.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kEnableModelQualityDogfoodLogging[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/optimization_guide_logger.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDebugLoggingEnabled[];
// Returns true if debug logs are enabled for the optimization guide.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsDebugLogsEnabled();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
