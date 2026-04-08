// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_

#include <string>

#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace omnibox {

std::string GetToolModeString(omnibox::ToolMode mode);

std::string GetModelModeString(omnibox::ModelMode mode);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_
