// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/history/core/browser/features.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

ntp_tiles::NTPTileImpression MakeNTPTileImpression(
    const most_visited::mojom::MostVisitedTile& tile,
    uint32_t index) {
  return ntp_tiles::NTPTileImpression(
      /*index=*/index,
      /*source=*/static_cast<ntp_tiles::TileSource>(tile.source),
      /*title_source=*/
      static_cast<ntp_tiles::TileTitleSource>(tile.title_source),
      /*visual_type=*/
      ntp_tiles::TileVisualType::ICON_REAL /* unused on desktop */,
      /*icon_type=*/favicon_base::IconType::kInvalid /* unused on desktop */,
      /*url_for_rappor=*/GURL() /* unused */);
}

bool IsFromEnterpriseShortcut(ntp_tiles::TileSource source) {
  return source == ntp_tiles::TileSource::ENTERPRISE_SHORTCUTS;
}

}  // namespace

MostVisitedHandler::MostVisitedHandler(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler,
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& ntp_url,
    const base::Time& ntp_navigation_start_time)
    : profile_(profile),
      most_visited_sites_(
          ChromeMostVisitedSitesFactory::NewForProfile(profile)),
      web_contents_(web_contents),
      logger_(profile, ntp_url, ntp_navigation_start_time),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      page_handler_(this, std::move(pending_page_handler)),
      page_(std::move(pending_page)) {
  most_visited_sites_->AddMostVisitedURLsObserver(
      this, ntp_tiles::kMaxNumMostVisited);

  web_app::WebAppProvider* web_app_provider_ =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (web_app_provider_) {
    preinstalled_web_app_observer_.Observe(
        &web_app_provider_->preinstalled_web_app_manager());
  }
}

MostVisitedHandler::~MostVisitedHandler() = default;

void MostVisitedHandler::EnableTileTypes(
    const ntp_tiles::MostVisitedSites::EnableTileTypesOptions& options) {
  most_visited_sites_->EnableTileTypes(options);
}

void MostVisitedHandler::SetShortcutsVisible(bool visible) {
  most_visited_sites_->SetShortcutsVisible(visible);
}

void MostVisitedHandler::AddMostVisitedTile(
    const GURL& url,
    const std::string& title,
    AddMostVisitedTileCallback callback) {
  DisableShortcutsAutoRemoval(profile_);
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    bool success =
        most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
    std::move(callback).Run(success);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_ADD,
                     base::TimeDelta() /* unused */);
  }
}

void MostVisitedHandler::DeleteMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile) {
  DisableShortcutsAutoRemoval(profile_);
  if (IsFromEnterpriseShortcut(tile->source)) {
    CHECK(most_visited_sites_->IsEnterpriseShortcutsEnabled());
    most_visited_sites_->DeleteEnterpriseShortcut(tile->url);
    logger_.LogEvent(NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_REMOVE,
                     base::TimeDelta() /* unused */);
    return;
  }

  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->DeleteCustomLink(tile->url);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_REMOVE,
                     base::TimeDelta() /* unused */);
  } else {
    most_visited_sites_->AddOrRemoveBlockedUrl(tile->url, true);
    last_blocklisted_ = tile->url;
  }
}

void MostVisitedHandler::RestoreMostVisitedDefaults(
    ntp_tiles::TileSource source) {
  if (IsFromEnterpriseShortcut(source)) {
    CHECK(most_visited_sites_->IsEnterpriseShortcutsEnabled());
    most_visited_sites_->RestoreEnterpriseShortcutsDefaults();
    logger_.LogEvent(NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_RESTORE_ALL,
                     base::TimeDelta() /* unused */);
    return;
  }

  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->UninitializeCustomLinks();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL,
                     base::TimeDelta() /* unused */);
  } else {
    most_visited_sites_->ClearBlockedUrls();
  }
}

void MostVisitedHandler::ReorderMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile,
    uint8_t new_pos) {
  DisableShortcutsAutoRemoval(profile_);
  if (IsFromEnterpriseShortcut(tile->source)) {
    CHECK(most_visited_sites_->IsEnterpriseShortcutsEnabled());
    most_visited_sites_->ReorderEnterpriseShortcut(tile->url, new_pos);
  } else if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->ReorderCustomLink(tile->url, new_pos);
  }
}

void MostVisitedHandler::UndoMostVisitedAutoRemoval() {
  DisableShortcutsAutoRemoval(profile_);
  // Set the pref to true to show the shortcuts.
  profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
}

void MostVisitedHandler::UndoMostVisitedTileAction(
    ntp_tiles::TileSource source) {
  if (IsFromEnterpriseShortcut(source)) {
    CHECK(most_visited_sites_->IsEnterpriseShortcutsEnabled());
    logger_.LogEvent(NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_UNDO,
                     base::TimeDelta() /* unused */);
    most_visited_sites_->UndoEnterpriseShortcutAction();
    return;
  }

  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->UndoCustomLinkAction();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_UNDO,
                     base::TimeDelta() /* unused */);
  } else if (last_blocklisted_.is_valid()) {
    most_visited_sites_->AddOrRemoveBlockedUrl(last_blocklisted_, false);
    last_blocklisted_ = GURL();
  }
}

void MostVisitedHandler::UpdateMostVisitedInfo() {
  most_visited_sites_->RefreshTiles();
}

void MostVisitedHandler::UpdateMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile,
    const GURL& new_url,
    const std::string& new_title,
    UpdateMostVisitedTileCallback callback) {
  DisableShortcutsAutoRemoval(profile_);
  if (IsFromEnterpriseShortcut(tile->source)) {
    CHECK(most_visited_sites_->IsEnterpriseShortcutsEnabled());
    bool success = most_visited_sites_->UpdateEnterpriseShortcut(
        tile->url, base::UTF8ToUTF16(new_title));
    std::move(callback).Run(success);
    logger_.LogEvent(NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_UPDATE,
                     base::TimeDelta() /* unused */);
  } else if (most_visited_sites_->IsCustomLinksEnabled()) {
    bool success = most_visited_sites_->UpdateCustomLink(
        tile->url, new_url != tile->url ? new_url : GURL(),
        base::UTF8ToUTF16(new_title));
    std::move(callback).Run(success);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_UPDATE,
                     base::TimeDelta() /* unused */);
  }
}

void MostVisitedHandler::OnMostVisitedTilesRendered(
    std::vector<most_visited::mojom::MostVisitedTilePtr> tiles,
    double time) {
  // Update staleness info on tiles rendered.
  UpdateShortcutsStaleness(profile_);
  for (size_t i = 0; i < tiles.size(); i++) {
    logger_.LogMostVisitedImpression(MakeNTPTileImpression(*tiles[i], i));
  }
  // This call flushes all most visited impression logs to UMA histograms.
  // Therefore, it must come last.
  logger_.LogMostVisitedLoaded(
      base::Time::FromMillisecondsSinceUnixEpoch(time) -
          ntp_navigation_start_time_,
      most_visited_sites_->IsTopSitesEnabled(),
      most_visited_sites_->IsCustomLinksEnabled(),
      most_visited_sites_->IsEnterpriseShortcutsEnabled(),
      most_visited_sites_->IsShortcutsVisible());
}

void MostVisitedHandler::OnMostVisitedTileNavigation(
    most_visited::mojom::MostVisitedTilePtr tile,
    uint32_t index,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  DisableShortcutsAutoRemoval(profile_);
  logger_.LogMostVisitedNavigation(MakeNTPTileImpression(*tile, index));

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  // Clicks on the MV tiles should be treated as if the user clicked on a
  // bookmark. This is consistent with Android's native implementation and
  // ensures the visit count for the MV entry is updated.
  // Use a link transition for query tiles, e.g., repeatable queries, so that
  // their visit count is not updated by this navigation. Otherwise duplicate
  // query tiles could also be offered as most visited.
  // |is_query_tile| can be true only when history::kOrganicRepeatableQueries
  // is enabled.
  base::OnceCallback<void(content::NavigationHandle&)>
      navigation_handle_callback =
          base::BindRepeating(&page_load_metrics::NavigationHandleUserData::
                                  AttachNewTabPageNavigationHandleUserData);
  web_contents_->OpenURL(
      content::OpenURLParams(tile->url, content::Referrer(), disposition,
                             tile->is_query_tile
                                 ? ui::PAGE_TRANSITION_LINK
                                 : ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                             /*is_renderer_initiated=*/false),
      std::move(navigation_handle_callback));
}

void MostVisitedHandler::GetMostVisitedExpandedState(
    GetMostVisitedExpandedStateCallback callback) {
  std::move(callback).Run(
      profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles));
}

void MostVisitedHandler::SetMostVisitedExpandedState(bool is_expanded) {
  DisableShortcutsAutoRemoval(profile_);
  profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles,
                                   is_expanded);
}

void MostVisitedHandler::PrerenderMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile) {
  if (!base::FeatureList::IsEnabled(
          features::kNewTabPageTriggerForPrerender2)) {
    page_handler_.ReportBadMessage(
        "PrerenderMostVisitedTile is only expected to be called "
        "when kNewTabPageTriggerForPrerender2 is true.");
    return;
  }

  auto* const preload_manager = GetNewTabPagePreloadPipelineManager();
  if (!preload_manager) {
    return;
  }

  preload_manager->StartPrerender(
      tile->url,
      chrome_preloading_predictor::kMouseHoverOrMouseDownOnNewTabPage);
}

void MostVisitedHandler::PrefetchMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile) {
  if (!base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrefetch)) {
    page_handler_.ReportBadMessage(
        "PrefetchMostVisitedTile is only expected to be called "
        "when kNewTabPageTriggerForPrefetch is true.");
    return;
  }

  auto* const preload_manager = GetNewTabPagePreloadPipelineManager();
  if (!preload_manager) {
    return;
  }

  preload_manager->StartPrefetch(tile->url);
}

void MostVisitedHandler::PreconnectMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile) {
  if (!base::FeatureList::IsEnabled(
          features::kNewTabPageTriggerForPrerender2)) {
    page_handler_.ReportBadMessage(
        "PreconnectMostVisitedTile is only expected to be called "
        "when kNewTabPageTriggerForPrerender2 is true.");
    return;
  }

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                          tile->url,
                                          predictors::HintOrigin::NEW_TAB_PAGE,
                                          /*preconnectable=*/true);
  }
}

void MostVisitedHandler::CancelPrerender() {
  if (!base::FeatureList::IsEnabled(
          features::kNewTabPageTriggerForPrerender2)) {
    page_handler_.ReportBadMessage(
        "CancelPrerender is only expected to be called "
        "when kNewTabPageTriggerForPrerender2 is true.");
    return;
  }

  auto* const preload_manager = GetNewTabPagePreloadPipelineManager();
  if (!preload_manager) {
    return;
  }
  preload_manager->ResetPrerender();
}

void MostVisitedHandler::OnURLsAvailable(
    bool is_user_triggered,
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  auto result = most_visited::mojom::MostVisitedInfo::New();
  std::vector<most_visited::mojom::MostVisitedTilePtr> tiles;
  for (auto& tile : sections.at(ntp_tiles::SectionType::PERSONALIZED)) {
    auto value = most_visited::mojom::MostVisitedTile::New();
    if (tile.title.empty()) {
      value->title = tile.url.spec();
      value->title_direction = base::i18n::LEFT_TO_RIGHT;
    } else {
      value->title = base::UTF16ToUTF8(tile.title);
      value->title_direction =
          base::i18n::GetFirstStrongCharacterDirection(tile.title);
    }
    value->url = tile.url;
    value->source = tile.source;
    value->title_source = static_cast<int32_t>(tile.title_source);
    value->is_query_tile =
        base::FeatureList::IsEnabled(history::kOrganicRepeatableQueries) &&
        template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            tile.url);
    value->allow_user_edit = tile.allow_user_edit;
    value->allow_user_delete = tile.allow_user_delete;
    tiles.push_back(std::move(value));
  }
  result->tiles = std::move(tiles);
  result->custom_links_enabled = most_visited_sites_->IsCustomLinksEnabled();
  result->enterprise_shortcuts_enabled =
      most_visited_sites_->IsEnterpriseShortcutsEnabled();
  result->visible = most_visited_sites_->IsShortcutsVisible();
  page_->SetMostVisitedInfo(std::move(result));
}

void MostVisitedHandler::OnIconMadeAvailable(const GURL& site_url) {}

NewTabPagePreloadPipelineManager*
MostVisitedHandler::GetNewTabPagePreloadPipelineManager() {
  tabs::TabInterface* tab = webui::GetTabInterface(web_contents_);
  return tab ? tab->GetTabFeatures()->new_tab_page_preload_pipeline_manager()
             : nullptr;
}

void MostVisitedHandler::OnMigrationRun() {
  most_visited_sites_->RefreshTiles();
}

void MostVisitedHandler::OnDestroyed() {
  if (preinstalled_web_app_observer_.IsObserving()) {
    preinstalled_web_app_observer_.Reset();
  }
}
