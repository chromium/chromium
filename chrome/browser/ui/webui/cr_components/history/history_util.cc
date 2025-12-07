// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history/history_util.h"

#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "chrome/grit/locale_settings.h"
#include "components/browsing_data/core/features.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

// Static
content::WebUIDataSource* HistoryUtil::PopulateCommonSourceForHistory(
    content::WebUIDataSource* source,
    Profile* profile) {
  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"ariaRoleDescription", IDS_HISTORY_ARIA_ROLE_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
      {"cancel", IDS_CANCEL},
      {"clearBrowsingData", IDS_CLEAR_BROWSING_DATA_TITLE},
      {"clearBrowsingDataLinkTooltip", IDS_SETTINGS_OPENS_IN_NEW_TAB},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"collapseSessionButton", IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION},
      {"delete", IDS_HISTORY_DELETE},
      {"deleteSuccess", IDS_HISTORY_REMOVE_PAGE_SUCCESS},
      {"deleteConfirm", IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON},
      {"deleteSession", IDS_HISTORY_OTHER_SESSIONS_HIDE_FOR_NOW},
      {"deleteWarning", IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING},
      {"entrySummary", IDS_HISTORY_ENTRY_SUMMARY},
      {"expandSessionButton", IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION},
      {"foundSearchResults", IDS_HISTORY_FOUND_SEARCH_RESULTS},
      {"actorTaskTooltip", IDS_ACTOR_TASK},
      {"historyMenuButton", IDS_HISTORY_HISTORY_MENU_DESCRIPTION},
      {"historyMenuItem", IDS_HISTORY_HISTORY_MENU_ITEM},
      {"itemsSelected", IDS_HISTORY_ITEMS_SELECTED},
      {"itemsUnselected", IDS_HISTORY_ITEMS_UNSELECTED},
      {"loading", IDS_HISTORY_LOADING},
      {"menu", IDS_MENU},
      {"moreFromSite", IDS_HISTORY_MORE_FROM_SITE},
      {"openAll", IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL},
      {"openSelected", IDS_HISTORY_OPEN},
      {"openTabsMenuItem", IDS_HISTORY_OPEN_TABS_MENU_ITEM},
      {"noResults", IDS_HISTORY_NO_RESULTS},
      {"noSearchResults", IDS_HISTORY_NO_SEARCH_RESULTS},
      {"removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK},
      {"removeFromHistory", IDS_HISTORY_REMOVE_PAGE},
      {"removeSelected", IDS_HISTORY_REMOVE_SELECTED_ITEMS},
      {"searchPrompt", IDS_HISTORY_SEARCH_PROMPT},
      {"searchResult", IDS_HISTORY_SEARCH_RESULT},
      {"searchResults", IDS_HISTORY_SEARCH_RESULTS},
      {"searchResultExactMatch", IDS_HISTORY_SEARCH_EXACT_MATCH_RESULT},
      {"searchResultExactMatches", IDS_HISTORY_SEARCH_EXACT_MATCH_RESULTS},
      {"title", IDS_HISTORY_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  PrefService* prefs = profile->GetPrefs();
  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);

  source->AddBoolean("isGuestSession", profile->IsGuestSession());
  source->AddBoolean("isSignInAllowed",
                     prefs->GetBoolean(prefs::kSigninAllowed));

  source->AddBoolean(
      "enableBrowsingHistoryActorIntegrationM1",
      browsing_data::features::IsBrowsingHistoryActorIntegrationM1Enabled());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  source->AddInteger(kSignInStateKey, static_cast<int>(GetHistorySignInState(
                                          identity_manager, sync_service)));

  source->AddInteger(
      "lastSelectedTab",
      prefs->GetInteger(history_clusters::prefs::kLastSelectedTab));

  history_embeddings::PopulateSourceForWebUI(source, profile);

  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"historyEmbeddingsAnswersSearchAlternativePrompt1",
       IDS_HISTORY_EMBEDDINGS_SEARCH_ANSWERS_ALTERNATIVE_PROMPT_1},
      {"historyEmbeddingsAnswersSearchAlternativePrompt2",
       IDS_HISTORY_EMBEDDINGS_SEARCH_ANSWERS_ALTERNATIVE_PROMPT_2},
      {"historyEmbeddingsAnswersSearchAlternativePrompt3",
       IDS_HISTORY_EMBEDDINGS_SEARCH_ANSWERS_ALTERNATIVE_PROMPT_3},
      {"historyEmbeddingsAnswersSearchAlternativePrompt4",
       IDS_HISTORY_EMBEDDINGS_SEARCH_ANSWERS_ALTERNATIVE_PROMPT_4},
      {"historyEmbeddingsShowByLabel",
       IDS_HISTORY_EMBEDDINGS_SHOW_BY_ARIA_LABEL},
      {"historyEmbeddingsShowByDate", IDS_HISTORY_EMBEDDINGS_SHOW_BY_DATE},
      {"historyEmbeddingsShowByGroup", IDS_HISTORY_EMBEDDINGS_SHOW_BY_GROUP},
      {"historyEmbeddingsSuggestion1", IDS_HISTORY_EMBEDDINGS_SUGGESTION_1},
      {"historyEmbeddingsSuggestion2", IDS_HISTORY_EMBEDDINGS_SUGGESTION_2},
      {"historyEmbeddingsSuggestion3", IDS_HISTORY_EMBEDDINGS_SUGGESTION_3},
      {"historyEmbeddingsSuggestion1AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_1_ARIA_LABEL},
      {"historyEmbeddingsSuggestion2AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_2_ARIA_LABEL},
      {"historyEmbeddingsSuggestion3AriaLabel",
       IDS_HISTORY_EMBEDDINGS_SUGGESTION_3_ARIA_LABEL},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);

  webui::SetupWebUIDataSource(source, kHistoryResources,
                              IDR_HISTORY_HISTORY_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  return source;
}
