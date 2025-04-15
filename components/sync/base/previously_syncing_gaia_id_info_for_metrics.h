// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_
#define COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_

namespace syncer {

enum class PreviouslySyncingGaiaIdInfoForMetrics {
  // Information not available or current state doesn't fall within any of the
  // buckets listed below.
  kUnspecified = 0,
  kNotEnoughInformationToTell = 1,
  kSyncFeatureNeverPreviouslyTurnedOn = 2,
  kCurrentGaiaIdMatchesPreviousWithSyncFeatureOn = 3,
  kCurrentGaiaIdIfDiffersPreviousWithSyncFeatureOn = 4,
  kMaxValue = kCurrentGaiaIdIfDiffersPreviousWithSyncFeatureOn
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_
