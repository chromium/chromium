// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_handler.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/ntp_tiles/constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/color_utils.h"

NewTabPageThirdPartyHandler::NewTabPageThirdPartyHandler(
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    const base::Time& ntp_navigation_start_time)
    : most_visited_sites_(
          ChromeMostVisitedSitesFactory::NewForProfile(profile)),
      profile_(profile),
      web_contents_(web_contents),
      logger_(profile, GURL(chrome::kChromeUINewTabPageThirdPartyURL)),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  most_visited_sites_->SetMostVisitedURLsObserver(
      this, ntp_tiles::kMaxNumMostVisited);
  most_visited_sites_->EnableCustomLinks(false);
  // Listen for theme installation.
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);
}

NewTabPageThirdPartyHandler::~NewTabPageThirdPartyHandler() {
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void NewTabPageThirdPartyHandler::DeleteMostVisitedTile(const GURL& url) {
  most_visited_sites_->AddOrRemoveBlockedUrl(url, true);
  last_blocklisted_ = url;
}

void NewTabPageThirdPartyHandler::RestoreMostVisitedDefaults() {
  most_visited_sites_->ClearBlockedUrls();
}

void NewTabPageThirdPartyHandler::UndoMostVisitedTileAction() {
  most_visited_sites_->AddOrRemoveBlockedUrl(last_blocklisted_, false);
  last_blocklisted_ = GURL();
}

void NewTabPageThirdPartyHandler::UpdateMostVisitedTiles() {
  NotifyAboutMostVisitedTiles();
}

void NewTabPageThirdPartyHandler::UpdateTheme() {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::OnMostVisitedTilesRendered(
    std::vector<new_tab_page_third_party::mojom::MostVisitedTilePtr> tiles,
    double time) {
  for (size_t i = 0; i < tiles.size(); i++) {
    logger_.LogMostVisitedImpression(ntp_tiles::NTPTileImpression(
        /*index=*/i,
        /*source=*/static_cast<ntp_tiles::TileSource>(tiles[i]->source),
        /*title_source=*/
        static_cast<ntp_tiles::TileTitleSource>(tiles[i]->title_source),
        /*visual_type=*/
        ntp_tiles::TileVisualType::ICON_REAL /* unused on desktop */,
        /*icon_type=*/favicon_base::IconType::kInvalid /* unused on desktop */,
        /*data_generation_time=*/tiles[i]->data_generation_time,
        /*url_for_rappor=*/GURL() /* unused */));
  }
  // This call flushes all most visited impression logs to UMA histograms.
  // Therefore, it must come last.
  logger_.LogEvent(NTP_ALL_TILES_LOADED,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageThirdPartyHandler::OnMostVisitedTileNavigation(
    new_tab_page_third_party::mojom::MostVisitedTilePtr tile,
    uint32_t index,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  logger_.LogMostVisitedNavigation(ntp_tiles::NTPTileImpression(
      /*index=*/index,
      /*source=*/static_cast<ntp_tiles::TileSource>(tile->source),
      /*title_source=*/
      static_cast<ntp_tiles::TileTitleSource>(tile->title_source),
      /*visual_type=*/
      ntp_tiles::TileVisualType::ICON_REAL /* unused on desktop */,
      /*icon_type=*/favicon_base::IconType::kInvalid /* unused on desktop */,
      /*data_generation_time=*/tile->data_generation_time,
      /*url_for_rappor=*/GURL() /* unused */));

  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpHandleMostVisitedNavigationExplicitly))
    return;

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  // Clicks on the MV tiles should be treated as if the user clicked on a
  // bookmark. This is consistent with Android's native implementation and
  // ensures the visit count for the MV entry is updated.
  // Use a link transition for query tiles, e.g., repeatable queries, so that
  // their visit count is not updated by this navigation. Otherwise duplicate
  // query tiles could also be offered as most visited.
  // |is_query_tile| can be true only when ntp_features::kNtpRepeatableQueries
  // is enabled.
  web_contents_->OpenURL(content::OpenURLParams(
      tile->url, content::Referrer(), disposition,
      tile->is_query_tile ? ui::PAGE_TRANSITION_LINK
                          : ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));
}

void NewTabPageThirdPartyHandler::OnThemeChanged() {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  most_visited_tiles_ = sections.at(ntp_tiles::SectionType::PERSONALIZED);
  NotifyAboutMostVisitedTiles();
}

void NewTabPageThirdPartyHandler::OnIconMadeAvailable(const GURL& site_url) {}

void NewTabPageThirdPartyHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::NotifyAboutMostVisitedTiles() {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  std::vector<new_tab_page_third_party::mojom::MostVisitedTilePtr> tiles;
  for (auto& tile : most_visited_tiles_) {
    auto value = new_tab_page_third_party::mojom::MostVisitedTile::New();
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
    value->data_generation_time = tile.data_generation_time;
    value->is_query_tile =
        base::FeatureList::IsEnabled(ntp_features::kNtpRepeatableQueries) &&
        template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            tile.url);
    tiles.push_back(std::move(value));
  }
  page_->SetMostVisitedTiles(std::move(tiles));
}

void NewTabPageThirdPartyHandler::NotifyAboutTheme() {
  auto theme = new_tab_page_third_party::mojom::Theme::New();
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  theme->shortcut_background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_SHORTCUT);
  theme->shortcut_text_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
  theme->is_dark = !color_utils::IsDark(theme->shortcut_text_color);
  theme->shortcut_use_white_tile_icon =
      color_utils::IsDark(theme->shortcut_background_color);
  theme->shortcut_use_title_pill = false;
  theme->color_background = color_utils::SkColorToRgbaString(
      GetThemeColor(webui::GetNativeTheme(web_contents_), theme_provider,
                    ThemeProperties::COLOR_NTP_BACKGROUND));
  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    theme->background_tiling = GetNewTabBackgroundTilingCSS(theme_provider);
    theme->background_position = GetNewTabBackgroundPositionCSS(theme_provider);
    theme->has_custom_background =
        theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND);
    theme->id = profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
    theme->shortcut_use_title_pill = true;
  }
  page_->SetTheme(std::move(theme));
}
