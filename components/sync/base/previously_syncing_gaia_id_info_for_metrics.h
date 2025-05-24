// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_
#define COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_

namespace syncer {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with the homonym enum
// in tools/metrics/histograms/metadata/sync/enums.xml.
// Exposed in the header file for testing.
// LINT.IfChange(PreviouslySyncingGaiaIdInfoForMetrics)
enum class PreviouslySyncingGaiaIdInfoForMetrics {
  // Information not available or current state doesn't fall within any of the
  // buckets listed below. Note that this value is also used to represent
  // irrelevant scenarios such as local sync (roaming profiles) being enabled
  // or datatypes being reconfigured as a result of the user customizing sync
  // settings.
  kUnspecified = 0,
  // Deprecated: kNotEnoughInformationToTell = 1,
  kSyncFeatureNeverPreviouslyTurnedOn = 2,
  kCurrentGaiaIdMatchesPreviousWithSyncFeatureOn = 3,
  kCurrentGaiaIdIfDiffersPreviousWithSyncFeatureOn = 4,
  kMaxValue = kCurrentGaiaIdIfDiffersPreviousWithSyncFeatureOn
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:PreviouslySyncingGaiaIdInfoForMetrics)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PREVIOUSLY_SYNCING_GAIA_ID_INFO_FOR_METRICS_H_
