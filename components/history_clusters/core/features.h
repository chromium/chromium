// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace history_clusters {

// Features

namespace internal {

// Enables Journeys in the Chrome History WebUI. This flag shouldn't be checked
// directly. Instead use `IsJourneysEnabled()` for the system language filter.
extern const base::Feature kJourneys;

// Enables labelling of Journeys in UI.
extern const base::Feature kJourneysLabels;

// Enables the Journeys Omnibox Action chip. `kJourneys` must also be enabled
// for this to take effect.
extern const base::Feature kOmniboxAction;

// Enables debug info in non-user-visible surfaces, like Chrome Inspector.
// Does nothing if `kJourneys` is disabled.
extern const base::Feature kNonUserVisibleDebug;

// Enables debug info in user-visible surfaces, like the actual WebUI page.
// Does nothing if `kJourneys` is disabled.
extern const base::Feature kUserVisibleDebug;

// Enables persisting context annotations in the History DB. They are always
// calculated anyways. This just enables storing them. This is expected to be
// enabled for all users shortly. This just provides a killswitch.

// This flag is to enable us to turn on persisting context annotations WITHOUT
// exposing the Memories UI in general. If EITHER this flag or `kJourneys` is
// enabled, users will have context annotations persisted into their History DB.
extern const base::Feature kPersistContextAnnotationsInHistoryDb;

// Enables the history clusters internals page.
extern const base::Feature kHistoryClustersInternalsPage;

// Enables use of task runner with trait CONTINUE_ON_SHUTDOWN.
extern const base::Feature kHistoryClustersUseContinueOnShutdown;

}  // namespace internal

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
