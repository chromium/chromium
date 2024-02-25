// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/url_constants.h"
#include "base/feature_list.h"
#include "components/history_clusters/core/features.h"

namespace history_clusters {

const char* GetChromeUIHistoryClustersURL() {
  return base::FeatureList::IsEnabled(history_clusters::kRenameJourneys)
             ? "chrome://history/grouped"
             : "chrome://history/journeys";
}

}  // namespace history_clusters
