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

extern const char kSegmentationPlatformRefreshResultsSwitch[];

// The timestamp before which all samples were compacted and future compactions
// need to only check for days after it.
extern const char kSegmentationLastDBCompactionTimePref[];
}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_CONSTANTS_H_
