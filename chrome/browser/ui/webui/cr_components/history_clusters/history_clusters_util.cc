// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"

#include "base/feature_list.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/page_image_service/features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"

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
  source->AddBoolean(
      "isHistoryClustersEnabled",
      history_clusters_service &&
          history_clusters_service->is_journeys_feature_flag_enabled());
  const bool journeys_is_managed =
      prefs->IsManagedPreference(history_clusters::prefs::kVisible);
  // History clusters are always visible unless the visibility prefs is
  // set to false by policy.
  source->AddBoolean(kIsHistoryClustersVisibleKey,
                     prefs->GetBoolean(history_clusters::prefs::kVisible) ||
                         !journeys_is_managed);
  source->AddBoolean(kIsHistoryClustersVisibleManagedByPolicyKey,
                     journeys_is_managed);
  source->AddBoolean("isHistoryClustersDebug",
                     history_clusters::GetConfig().user_visible_debug);
  source->AddBoolean(
      "isHistoryClustersImagesEnabled",
      history_clusters::GetConfig().images &&
          base::FeatureList::IsEnabled(page_image_service::kImageService));

  source->AddBoolean("isHistoryClustersImageCover",
                     history_clusters::GetConfig().images_cover);

  static constexpr webui::LocalizedString kHistoryClustersStrings[] = {
      {"actionMenuDescription", IDS_HISTORY_CLUSTERS_ACTION_MENU_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
      {"cancel", IDS_CANCEL},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"deleteConfirm",
       IDS_HISTORY_CLUSTERS_DELETE_PRIOR_VISITS_CONFIRM_BUTTON},
      {"deleteWarning", IDS_HISTORY_CLUSTERS_DELETE_PRIOR_VISITS_WARNING},
      {"hideFromCluster", IDS_HISTORY_CLUSTERS_HIDE_PAGE},
      {"hideAllVisits", IDS_HISTORY_CLUSTERS_HIDE_VISITS},
      {"historyClustersTabLabel", IDS_HISTORY_CLUSTERS_BY_GROUP_TAB_LABEL},
      {"historyListTabLabel", IDS_HISTORY_CLUSTERS_BY_DATE_TAB_LABEL},
      {"loadMoreButtonLabel", IDS_HISTORY_CLUSTERS_LOAD_MORE_BUTTON_LABEL},
      {"historyClustersNoResults", IDS_HISTORY_CLUSTERS_NO_RESULTS},
      {"noSearchResults", IDS_HISTORY_CLUSTERS_NO_SEARCH_RESULTS},
      {"openAllInTabGroup", IDS_HISTORY_CLUSTERS_OPEN_ALL_IN_TABGROUP},
      {"relatedSearchesHeader", IDS_HISTORY_CLUSTERS_RELATED_SEARCHES_HEADER},
      {"removeAllFromHistory", IDS_HISTORY_CLUSTERS_REMOVE_ALL_ITEMS},
      {"removeFromHistory", IDS_HISTORY_CLUSTERS_REMOVE_PAGE},
      {"removeFromHistoryToast", IDS_HISTORY_CLUSTERS_REMOVE_ITEM_TOAST},
      {"removeSelected", IDS_HISTORY_CLUSTERS_REMOVE_SELECTED_ITEMS},
      {"savedInTabGroup", IDS_HISTORY_CLUSTERS_SAVED_IN_TABGROUP_LABEL},
      {"historyClustersSearchPrompt", IDS_HISTORY_SEARCH_PROMPT},
      {"toggleButtonLabelLess", IDS_HISTORY_CLUSTERS_SHOW_LESS_BUTTON_LABEL},
      {"toggleButtonLabelMore", IDS_HISTORY_CLUSTERS_SHOW_MORE_BUTTON_LABEL},
  };
  source->AddLocalizedStrings(kHistoryClustersStrings);

  return;
}
