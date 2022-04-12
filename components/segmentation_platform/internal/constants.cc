// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

const char kSegmentationResultPref[] =
    "segmentation_platform.segmentation_result";

const char kSegmentationUkmMostRecentAllowedTimeKey[] =
    "segmentation_platform.ukm_most_recent_allowed_time_key";

// The segmentation platform will ignore all the valid results from previous
// model executions, and re-run all the models and recompute segment selections.
// Used for testing the model execution locally.
const char kSegmentationPlatformRefreshResultsSwitch[] =
    "segmentation-platform-refresh-results";

}  // namespace segmentation_platform
