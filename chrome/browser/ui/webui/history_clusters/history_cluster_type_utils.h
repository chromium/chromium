// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTER_TYPE_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTER_TYPE_UTILS_H_

#include "components/history_clusters/public/mojom/history_cluster_types.mojom-forward.h"

class Profile;

namespace history {
struct Cluster;
}  // namespace history

namespace history_clusters {

// Creates a `mojom::Cluster` from a `history_clusters::Cluster`.
mojom::ClusterPtr ClusterToMojom(Profile* profile,
                                 const history::Cluster cluster);

}  // namespace history_clusters

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTER_TYPE_UTILS_H_
