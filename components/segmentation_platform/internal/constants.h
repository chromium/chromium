// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_CONSTANTS_H_

namespace segmentation_platform {

// The path to the pref storing the segmentation result.
extern const char kSegmentationResultPref[];

// The path to the prefs storing results for all the clients, supporting multi
// output models.
extern const char kSegmentationClientResultPrefs[];

// The path to the pref storing when UKM are allowed recently.
extern const char kSegmentationUkmMostRecentAllowedTimeKey[];

// Last metrics collection time for the segmentation platform.
extern const char kSegmentationLastCollectionTimePref[];

// The segmentation platform will ignore all the valid results from previous
// model executions, and re-run all the models and recompute segment selections.
// Used for automated testing as well testing the model execution locally.
extern const char kSegmentationPlatformRefreshResultsSwitch[];

// All the model executed at startup would run without any delay.
// Used for automated testing as well testing the model execution locally.
extern const char kSegmentationPlatformDisableModelExecutionDelaySwitch[];

// The timestamp before which all samples were compacted and future compactions
// need to only check for days after it.
extern const char kSegmentationLastDBCompactionTimePref[];

// The timestamp since when the UMA signals are stored in SQL database.
extern const char kSegmentationUmaSqlDatabaseStartTimePref[];

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_CONSTANTS_H_
