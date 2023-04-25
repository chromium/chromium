// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece_forward.h"
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

BASE_FEATURE(kJourneys, "Journeys", enabled_by_default_desktop_only);

BASE_FEATURE(kJourneysLabels,
             "JourneysLabel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kJourneysImages,
             "JourneysImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kJourneysImagesCover{
    &kJourneysImages, "JourneysImagesCover", true};

BASE_FEATURE(kPersistedClusters,
             "HistoryClustersPersistedClusters",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxAction,
             "JourneysOmniboxAction",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxHistoryClusterProvider,
             "JourneysOmniboxHistoryClusterProvider",
             enabled_by_default_desktop_only);

BASE_FEATURE(kNonUserVisibleDebug,
             "JourneysNonUserVisibleDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserVisibleDebug,
             "JourneysUserVisibleDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersistContextAnnotationsInHistoryDb,
             "JourneysPersistContextAnnotationsInHistoryDb",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersInternalsPage,
             "HistoryClustersInternalsPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersUseContinueOnShutdown,
             "HistoryClustersUseContinueOnShutdown",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersKeywordFiltering,
             "HistoryClustersKeywordFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersVisitDeduping,
             "HistoryClustersVisitDeduping",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kJourneysIncludeSyncedVisits,
             "JourneysIncludeSyncedVisits",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kJourneysPersistCachesToPrefs,
             "JourneysPersistCachesToPrefs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryClustersNavigationContextClustering,
             "HistoryClustersNavigationContextClustering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(manukh): Launched with chromium roll out in m114 3/29/23. Clean feature
//   code when m114 reaches stable 5/30.
BASE_FEATURE(kHideVisits,
             "HistoryClustersHideVisits",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseUrlForDisplayCache,
             "HistoryClustersUrlForDisplayCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kJourneysZeroStateFiltering,
             "JourneysZeroStateFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace internal

BASE_FEATURE(kSidePanelJourneys,
             "SidePanelJourneys",
             base::FEATURE_ENABLED_BY_DEFAULT);
// If enabled, and the main flag is also enabled, the Journeys omnibox
// entrypoints open Journeys in Side Panel rather than the History WebUI.
const base::FeatureParam<bool> kSidePanelJourneysOpensFromOmnibox{
    &kSidePanelJourneys, "SidePanelJourneysOpensFromOmnibox", true};

}  // namespace history_clusters
