// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

// Static
void HistoryClustersUtil::PopulateSource(content::WebUIDataSource* source,
                                         Profile* profile,
                                         bool in_side_panel) {
  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));
  source->AddBoolean("inSidePanel", in_side_panel);
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile);
  source->AddBoolean("isHistoryClustersEnabled",
                     history_clusters_service &&
                         history_clusters_service->IsJourneysEnabled());
  source->AddBoolean(kIsHistoryClustersVisibleKey,
                     prefs->GetBoolean(history_clusters::prefs::kVisible));
  source->AddBoolean(
      kIsHistoryClustersVisibleManagedByPolicyKey,
      prefs->IsManagedPreference(history_clusters::prefs::kVisible));
  source->AddBoolean("isHistoryClustersDebug",
                     history_clusters::GetConfig().user_visible_debug);

  static constexpr webui::LocalizedString kHistoryClustersStrings[] = {
      {"actionMenuDescription", IDS_HISTORY_CLUSTERS_ACTION_MENU_DESCRIPTION},
      {"disableHistoryClusters", IDS_HISTORY_CLUSTERS_DISABLE_MENU_ITEM_LABEL},
      {"enableHistoryClusters", IDS_HISTORY_CLUSTERS_ENABLE_MENU_ITEM_LABEL},
      {"historyClustersTabLabel", IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL},
      {"historyListTabLabel", IDS_HISTORY_CLUSTERS_LIST_TAB_LABEL},
      {"loadMoreButtonLabel", IDS_HISTORY_CLUSTERS_LOAD_MORE_BUTTON_LABEL},
      {"noSearchResults", IDS_HISTORY_CLUSTERS_NO_SEARCH_RESULTS},
      {"openAllInTabGroup", IDS_HISTORY_CLUSTERS_OPEN_ALL_IN_TABGROUP},
      {"relatedSearchesHeader", IDS_HISTORY_CLUSTERS_RELATED_SEARCHES_HEADER},
      {"removeAllFromHistory", IDS_HISTORY_CLUSTERS_REMOVE_ALL_ITEMS},
      {"removeFromHistory", IDS_HISTORY_CLUSTERS_REMOVE_PAGE},
      {"removeFromHistoryToast", IDS_HISTORY_CLUSTERS_REMOVE_ITEM_TOAST},
      {"savedInTabGroup", IDS_HISTORY_CLUSTERS_SAVED_IN_TABGROUP_LABEL},
      {"toggleButtonLabelLess", IDS_HISTORY_CLUSTERS_SHOW_LESS_BUTTON_LABEL},
      {"toggleButtonLabelMore", IDS_HISTORY_CLUSTERS_SHOW_MORE_BUTTON_LABEL},
  };
  source->AddLocalizedStrings(kHistoryClustersStrings);
  return;
}
