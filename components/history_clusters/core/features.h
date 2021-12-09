// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace history_clusters {

// Params & helpers functions

// Returns true if Journeys in the Chrome History WebUI is enabled.
// Callers with access to `HistoryClustersService` should use
// `HistoryClustersService::IsJourneysEnabled` which has precomputed this value
// with the g_browser_process locale. Renderer process callers will have to
// use this function directly.
bool IsJourneysEnabled(const std::string& application_locale);

// A comma (or colon) separated list of allowed locales and languages for which
// Journeys is enabled. If this string is empty, any application locale or
// language is allowed. If this string is non-empty, then the either the user's
// system locale or primary language subtag must match one of the elements for
// Journeys to be enabled.
//
// For example, "en,zh-TW" would mark English language users from any country,
// and Chinese language users from Taiwan as on the allowlist.
extern const base::FeatureParam<std::string> kLocaleOrLanguageAllowlist;

// The max number of visits to use for each clustering iteration. This limits
// the number of visits sent to the clustering backend per batch.
extern const base::FeatureParam<int> kMaxVisitsToCluster;

// The recency threshold controlling which visits will be clustered. This isn't
// the only factor; i.e. visits older than `MaxDaysToCluster()` may still be
// clustered. Only applies when using persisted visit context annotations; i.e.
// `kPersistContextAnnotationsInHistoryDb` is true.
extern const base::FeatureParam<int> kMaxDaysToCluster;

// A soft cap on the number of keyword phrases to cache. If 0, there is no
// limit.
extern const base::FeatureParam<int> kMaxKeywordPhrases;

// If enabled, updating clusters will persist the results to the history DB and
// accessing clusters will retrieve them from the history DB. If disabled,
// updating clusters is a no-op and accessing clusters will generate and return
// new clusters without persisting them.
extern const base::FeatureParam<bool> kPersistClustersInHistoryDb;

// Enables the on-device clustering backend. Enabled by default, since this is
// the production mode of the whole feature. The backend is only in official
// builds, so it won't work in unofficial builds.
extern const base::FeatureParam<bool> kUseOnDeviceClusteringBackend;

// If enabled, changes the History Clusters omnibox action text to be:
// "Resume your research" instead of "Resume your journey".
extern const base::FeatureParam<bool> kAlternateOmniboxActionText;

// If enabled, this is the min score that a visit needs to have to always be
// shown above the fold regardless of the number of visits already shown.
extern const base::FeatureParam<double> kMinScoreToAlwaysShowAboveTheFold;

// If enabled, this is the number of non-zero scored visits to always show
// above the fold regardless of score.
extern const base::FeatureParam<int> kNumVisitsToAlwaysShowAboveTheFold;

// Features

namespace internal {
// Enables Journeys in the Chrome History WebUI. This flag shouldn't be checked
// directly. Instead use `IsJourneysEnabled()` for the system language filter.
extern const base::Feature kJourneys;
}  // namespace internal

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

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_FEATURES_H_
