// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/privacy_budget_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_FEATURE(kIdentifiabilityStudyMetaExperiment,
             "IdentifiabilityStudyMetaExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double>
    kIdentifiabilityStudyMetaExperimentActivationProbability = {
        // The value -1, being outside the interval [0, 1], will be interpreted
        // as the default probability, which depends on the channel and is
        // encoded in
        // chrome/browser/privacy_budget/identifiability_study_state.cc.
        &kIdentifiabilityStudyMetaExperiment, "ActivationProbability",
        kIdentifiabilityStudyMetaExperimentDefaultActivationProbability};

BASE_FEATURE(kIdentifiabilityStudy,
             "IdentifiabilityStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kIdentifiabilityStudyGeneration = {
    &kIdentifiabilityStudy, "Gen", 0};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedMetrics = {
    &kIdentifiabilityStudy, "BlockedHashes", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedTypes = {
    &kIdentifiabilityStudy, "BlockedTypes", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyAllowedRandomTypes =
    {&kIdentifiabilityStudy, "AllowedRandomTypes", ""};

const base::FeatureParam<int> kIdentifiabilityStudyExpectedSurfaceCount = {
    &kIdentifiabilityStudy, "Rho", 0};

const base::FeatureParam<int> kIdentifiabilityStudyActiveSurfaceBudget = {
    &kIdentifiabilityStudy, "Max", kMaxIdentifiabilityStudyActiveSurfaceBudget};

const base::FeatureParam<std::string> kIdentifiabilityStudyPerHashCost = {
    &kIdentifiabilityStudy, "HashCost", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyPerTypeCost = {
    &kIdentifiabilityStudy, "TypeCost", ""};

const base::FeatureParam<std::string>
    kIdentifiabilityStudySurfaceEquivalenceClasses = {&kIdentifiabilityStudy,
                                                      "Classes", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlocks = {
    &kIdentifiabilityStudy, "Blocks", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockWeights = {
    &kIdentifiabilityStudy, "BlockWeights", ""};

}  // namespace features
