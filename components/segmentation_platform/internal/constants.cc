// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

const char kSegmentationResultPref[] =
    "segmentation_platform.segmentation_result";

const char kSegmentationUkmMostRecentAllowedTimeKey[] =
    "segmentation_platform.ukm_most_recent_allowed_time_key";

const char kSegmentationLastCollectionTimePref[] =
    "segmentation_platform.last_collection_time";

// The segmentation platform will ignore all the valid results from previous
// model executions, and re-run all the models and recompute segment selections.
// Used for testing the model execution locally.
const char kSegmentationPlatformRefreshResultsSwitch[] =
    "segmentation-platform-refresh-results";

const char kSegmentationLastDBCompactionTimePref[] =
    "segmentation_platform.last_db_compaction_time";

}  // namespace segmentation_platform
