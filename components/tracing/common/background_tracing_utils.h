// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"

namespace tracing {

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromCommandLine();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupPresetTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupFieldTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupSystemTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetBackgroundTracingOutputPath();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool HasBackgroundTracingOutputPath();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
