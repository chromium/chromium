// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_ADAPTIVE_TOOLBAR_MIGRATION_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_ADAPTIVE_TOOLBAR_MIGRATION_H_

#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/client_results.pb.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {
struct Config;

namespace pref_migration_utils {

proto::ClientResult CreateClientResultForAdaptiveToolbar(
    Config* config,
    const SelectedSegment& old_result);
}  // namespace pref_migration_utils
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_ADAPTIVE_TOOLBAR_MIGRATION_H_
