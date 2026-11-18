// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_H_
#define COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_H_

namespace variations {

class FeatureListCreatorClient;

// Creates the global `base::FeatureList` and sets up field trials.
// Runs the Chrome and iOS startup sequence:
//   1. Converts `about:flags` entries to command-line switches unless the
//      process command line has `--no-experiments`.
//   2. Updates command-line crash keys for switches and features.
//   3. Calls `FeatureListCreatorClient::GetOrCreateMetricsServicesManager()`.
//   4. Instantiates `base::FieldTrialList` and constructs `base::FeatureList`.
//   5. Registers `about:flags` feature variation parameters unless the
//      process command line has `--no-experiments`.
//   6. Calls `FeatureListCreatorClient::WillSetUpFieldTrials()`.
//   7. Calls `VariationsService::SetUpFieldTrials()` to apply extra feature
//      overrides and set the global `base::FeatureList`.
// Create Local State before `CreateFeatureList()`.
// Call `CreateFeatureList()` only once per process.
void CreateFeatureList(FeatureListCreatorClient& client);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FEATURE_LIST_CREATOR_FEATURE_LIST_CREATOR_H_
