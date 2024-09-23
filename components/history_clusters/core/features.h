// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_clusters {

// Features

// In the past, we put all features in the Config class for performance, and
// so all these are listed as "internal" and not to be checked directly.
// Since crbug.com/1341292 has been fixed we no longer need to do this, and can
// put features outside of this internal namespace. See bottom of this file.
namespace internal {

// Enables Journeys in the Chrome History WebUI. This flag shouldn't be checked
// directly. Instead use `IsJourneysEnabled()` for the system language filter.
BASE_DECLARE_FEATURE(kJourneys);

// Enables images for Journeys in UI.
BASE_DECLARE_FEATURE(kJourneysImages);

// Enables images to cover the full container for Journeys in UI.
extern const base::FeatureParam<bool> kJourneysImagesCover;

// Enables the Journeys Omnibox Action chip. `kJourneys` must also be enabled
// for this to take effect.
BASE_DECLARE_FEATURE(kOmniboxAction);

// Enables the `HistoryClusterProvider` to surface Journeys as a suggestion row
// instead of an action chip. Enabling this won't actually disable
// `kOmniboxAction` but for user experiments, the intent is to only have 1
// enabled. `kJourneys` must also be enabled for this to take effect.
BASE_DECLARE_FEATURE(kOmniboxHistoryClusterProvider);

// Enables debug info in non-user-visible surfaces, like Chrome Inspector.
// Does nothing if `kJourneys` is disabled.
BASE_DECLARE_FEATURE(kNonUserVisibleDebug);

// Enables debug info in user-visible surfaces, like the actual WebUI page.
// Does nothing if `kJourneys` is disabled.
BASE_DECLARE_FEATURE(kUserVisibleDebug);

// This flag is to enable us to turn on persisting context annotations WITHOUT
// exposing the Journeys UI in general. If EITHER this flag or `kJourneys` is
// enabled, users will have context annotations persisted into their History DB.
BASE_DECLARE_FEATURE(kPersistContextAnnotationsInHistoryDb);

// Enables the history clusters internals page.
BASE_DECLARE_FEATURE(kHistoryClustersInternalsPage);

// Enables use of task runner with trait CONTINUE_ON_SHUTDOWN.
BASE_DECLARE_FEATURE(kHistoryClustersUseContinueOnShutdown);

// Enables use of additional keyword filtering operations on clusters.
BASE_DECLARE_FEATURE(kHistoryClustersKeywordFiltering);

// Enables experimentation for how to dedupe visits in clusters.
BASE_DECLARE_FEATURE(kHistoryClustersVisitDeduping);

// Persist keyword caches via pref service.
BASE_DECLARE_FEATURE(kJourneysPersistCachesToPrefs);

// Enables context clustering to be performed at navigation time rather than in
// batches.
BASE_DECLARE_FEATURE(kHistoryClustersNavigationContextClustering);

// Enables Journeys creating new tab groups that have names derived from the
// cluster title. If disabled, new tab groups are anonymous.
BASE_DECLARE_FEATURE(kJourneysNamedNewTabGroups);

// Enables filtering of the zero-state Journeys WebUI.
BASE_DECLARE_FEATURE(kJourneysZeroStateFiltering);

// Order consistently with config.h.

}  // namespace internal

// The below features are NOT internal and NOT encapsulated in the Config class.
// At one point, we put everything in the Config class for performance, but
// since crbug.com/1341292 has been fixed, we no longer need to.

// Enables Side Panel Journeys.
BASE_DECLARE_FEATURE(kSidePanelJourneys);
extern const base::FeatureParam<bool> kSidePanelJourneysOpensFromOmnibox;
BASE_DECLARE_FEATURE(kSidePanelJourneysQueryless);

// Enables Side Panel Grouped History and WebUI Grouped History to find
// ungrouped visits as well when the user has typed a search query.
BASE_DECLARE_FEATURE(kSearchesFindUngroupedVisits);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
