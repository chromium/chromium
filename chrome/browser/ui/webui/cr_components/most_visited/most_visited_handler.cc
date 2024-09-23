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
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/history/core/browser/features.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
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

void MostVisitedHandler::EnableCustomLinks(bool enable) {
  most_visited_sites_->EnableCustomLinks(enable);
}

void MostVisitedHandler::SetShortcutsVisible(bool visible) {
  most_visited_sites_->SetShortcutsVisible(visible);
}

void MostVisitedHandler::AddMostVisitedTile(
    const GURL& url,
    const std::string& title,
    AddMostVisitedTileCallback callback) {
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    bool success =
        most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
    std::move(callback).Run(success);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_ADD,
                     base::TimeDelta() /* unused */);
  }
}

void MostVisitedHandler::DeleteMostVisitedTile(const GURL& url) {
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->DeleteCustomLink(url);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_REMOVE,
                     base::TimeDelta() /* unused */);
  } else {
    most_visited_sites_->AddOrRemoveBlockedUrl(url, true);
    last_blocklisted_ = url;
  }
}

void MostVisitedHandler::RestoreMostVisitedDefaults() {
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->UninitializeCustomLinks();
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL,
                     base::TimeDelta() /* unused */);
  } else {
    most_visited_sites_->ClearBlockedUrls();
  }
}

void MostVisitedHandler::ReorderMostVisitedTile(const GURL& url,
                                                uint8_t new_pos) {
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    most_visited_sites_->ReorderCustomLink(url, new_pos);
  }
}

void MostVisitedHandler::UndoMostVisitedTileAction() {
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
    const GURL& url,
    const GURL& new_url,
    const std::string& new_title,
    UpdateMostVisitedTileCallback callback) {
  if (most_visited_sites_->IsCustomLinksEnabled()) {
    bool success = most_visited_sites_->UpdateCustomLink(
        url, new_url != url ? new_url : GURL(), base::UTF8ToUTF16(new_title));
    std::move(callback).Run(success);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_UPDATE,
                     base::TimeDelta() /* unused */);
  }
}

void MostVisitedHandler::OnMostVisitedTilesRendered(
    std::vector<most_visited::mojom::MostVisitedTilePtr> tiles,
    double time) {
  for (size_t i = 0; i < tiles.size(); i++) {
    logger_.LogMostVisitedImpression(MakeNTPTileImpression(*tiles[i], i));
  }
  // This call flushes all most visited impression logs to UMA histograms.
  // Therefore, it must come last.
  logger_.LogMostVisitedLoaded(
      base::Time::FromMillisecondsSinceUnixEpoch(time) -
          ntp_navigation_start_time_,
      !most_visited_sites_->IsCustomLinksEnabled(),
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

void MostVisitedHandler::PrerenderMostVisitedTile(
    most_visited::mojom::MostVisitedTilePtr tile,
    bool is_hover_trigger) {
  if (!base::FeatureList::IsEnabled(
          features::kNewTabPageTriggerForPrerender2)) {
    page_handler_.ReportBadMessage(
        "PrerenderMostVisitedTile is only expected to be called "
        "when kNewTabPageTriggerForPrerender2 is true.");
    return;
  }

  if (is_hover_trigger &&
      !features::kPrerenderNewTabPageOnMouseHoverTrigger.Get()) {
    page_handler_.ReportBadMessage(
        "PrerenderMostVisitedTile by hovering is only expected to be called "
        "when kPrerenderNewTabPageOnMouseHoverTrigger is true.");
    return;
  }

  if (!is_hover_trigger &&
      !features::kPrerenderNewTabPageOnMousePressedTrigger.Get()) {
    page_handler_.ReportBadMessage(
        "PrerenderMostVisitedTile by pressing is only expected to be called "
        "when kPrerenderNewTabPageOnMousePressedTrigger is true.");
    return;
  }
  PrerenderManager::CreateForWebContents(web_contents_);
  auto* prerender_manager = PrerenderManager::FromWebContents(web_contents_);

  prerender_handle_ = prerender_manager->StartPrerenderNewTabPage(
      tile->url, is_hover_trigger
                     ? chrome_preloading_predictor::kMouseHoverOnNewTabPage
                     : chrome_preloading_predictor::kPointerDownOnNewTabPage);
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

  auto* prerender_manager = PrerenderManager::FromWebContents(web_contents_);
  prerender_manager->StopPrerenderNewTabPage(prerender_handle_);
  prerender_handle_ = nullptr;
}

void MostVisitedHandler::OnURLsAvailable(
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
    value->source = static_cast<int32_t>(tile.source);
    value->title_source = static_cast<int32_t>(tile.title_source);
    value->is_query_tile =
        base::FeatureList::IsEnabled(history::kOrganicRepeatableQueries) &&
        template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            tile.url);
    tiles.push_back(std::move(value));
  }
  result->tiles = std::move(tiles);
  result->custom_links_enabled = most_visited_sites_->IsCustomLinksEnabled();
  result->visible = most_visited_sites_->IsShortcutsVisible();
  page_->SetMostVisitedInfo(std::move(result));
}

void MostVisitedHandler::OnIconMadeAvailable(const GURL& site_url) {}

void MostVisitedHandler::OnMigrationRun() {
  most_visited_sites_->RefreshTiles();
}

void MostVisitedHandler::OnDestroyed() {
  if (preinstalled_web_app_observer_.IsObserving())
    preinstalled_web_app_observer_.Reset();
}
