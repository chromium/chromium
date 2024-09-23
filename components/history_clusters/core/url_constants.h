// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_URL_CONSTANTS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_URL_CONSTANTS_H_

namespace history_clusters {

// Returns the main History Clusters WebUI URL.
// TODO(crbug.com/40069898): Change to a const char array when feature check is
//  no longer needed.
const char* GetChromeUIHistoryClustersURL();

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_URL_CONSTANTS_H_
