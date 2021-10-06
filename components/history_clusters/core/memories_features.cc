// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace history_clusters {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if defined(OS_ANDROID) || defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

const base::FeatureParam<std::string> kRemoteModelEndpoint{
    &kRemoteModelForDebugging, "JourneysRemoteModelEndpoint", ""};

}  // namespace

GURL RemoteModelEndpoint() {
  return GURL(kRemoteModelEndpoint.Get());
}

const base::FeatureParam<std::string> kRemoteModelEndpointExperimentName{
    &kJourneys, "JourneysExperimentName", ""};

const base::FeatureParam<int> kMaxVisitsToCluster{
    &kJourneys, "JourneysMaxVisitsToCluster", 1000};

const base::FeatureParam<int> kMaxDaysToCluster{&kJourneys,
                                                "JourneysMaxDaysToCluster", 9};

const base::FeatureParam<bool> kPersistClustersInHistoryDb{
    &kJourneys, "JourneysPersistClustersInHistoryDb", false};

const base::FeatureParam<bool> kUseOnDeviceClusteringBackend{
    &kJourneys, "JourneysOnDeviceClusteringBackend", true};

// Default to true, as this this new alternate action text was recommended by
// our UX writers.
const base::FeatureParam<bool> kAlternateOmniboxActionText{
    &kOmniboxAction, "JourneysAlternateOmniboxActionText", true};

const base::Feature kJourneys{"Journeys", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxAction{"JourneysOmniboxAction",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNonUserVisibleDebug{"JourneysNonUserVisibleDebug",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserVisibleDebug{"JourneysUserVisibleDebug",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteModelForDebugging{"JourneysRemoteModelForDebugging",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPersistContextAnnotationsInHistoryDb{
    "JourneysPersistContextAnnotationsInHistoryDb",
    enabled_by_default_desktop_only};

}  // namespace history_clusters
