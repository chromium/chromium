// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_utils.h"

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace side_panel::history_clusters {

bool IsHistoryClustersSidePanelSupportedForProfile(Profile* profile) {
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile);
  return base::FeatureList::IsEnabled(::history_clusters::kSidePanelJourneys) &&
         history_clusters_service &&
         history_clusters_service->IsJourneysEnabledAndVisible() &&
         !profile->IsIncognitoProfile() && !profile->IsGuestSession();
}

}  // namespace side_panel::history_clusters
