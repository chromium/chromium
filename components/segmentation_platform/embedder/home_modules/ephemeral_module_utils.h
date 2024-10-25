// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_MODULE_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_MODULE_UTILS_H_

#include <optional>

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// Returns the `CardSelectionInfo::ShowResult` of the ephemeral module that is
// forced to be shown or hidden, if any.
std::optional<CardSelectionInfo::ShowResult>
GetForcedEphemeralModuleShowResult();

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_MODULE_UTILS_H_
