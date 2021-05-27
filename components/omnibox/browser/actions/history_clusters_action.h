// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_

namespace history_clusters {
class HistoryClustersService;
}
class AutocompleteResult;

// If the feature is enabled, attaches any necessary History Clusters actions
// onto any relevant matches in `result`.
void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    AutocompleteResult& result);

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
