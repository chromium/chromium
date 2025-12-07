// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

}  // namespace

namespace internal {

BASE_FEATURE(kJourneys, enabled_by_default_desktop_only);

BASE_FEATURE(kJourneysImages, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kJourneysImagesCover{
    &kJourneysImages, "JourneysImagesCover", true};

BASE_FEATURE(kOmniboxHistoryClusterProvider,
             "JourneysOmniboxHistoryClusterProvider",
             enabled_by_default_desktop_only);

BASE_FEATURE(kNonUserVisibleDebug,
             "JourneysNonUserVisibleDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserVisibleDebug,
             "JourneysUserVisibleDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersInternalsPage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersVisitDeduping, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersNavigationContextClustering,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace internal

// Intended to be Enabled by default on Desktop and the flag left here as a
// killswitch.
BASE_FEATURE(kSearchesFindUngroupedVisits,
             "GroupedHistorySearchesFindUngroupedVisits",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace history_clusters
