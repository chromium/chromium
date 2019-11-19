// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "components/favicon/core/favicon_service.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

using FaviconResultMap = std::map<std::pair<GURL, favicon_base::IconType>,
                                  favicon_base::FaviconRawBitmapResult>;

struct IconTypeAndName {
  favicon_base::IconType type_enum;
  const char* type_name;
};

constexpr std::array<IconTypeAndName, 4> kIconTypesAndNames{{
    {favicon_base::IconType::kFavicon, "kFavicon"},
    {favicon_base::IconType::kTouchIcon, "kTouchIcon"},
    {favicon_base::IconType::kTouchPrecomposedIcon, "kTouchPrecomposedIcon"},
    {favicon_base::IconType::kWebManifestIcon, "kWebManifestIcon"},
}};

std::string FormatJson(const base::Value& value) {
  std::string pretty_printed;
  bool ok = base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty_printed);
  DCHECK(ok);
  return pretty_printed;
}

}  // namespace

NTPTilesInternalsMessageHandler::NTPTilesInternalsMessageHandler(
    favicon::FaviconService* favicon_service)
    : favicon_service_(favicon_service),
      client_(nullptr),
      site_count_(ntp_tiles::kMaxNumMostVisited) {}

NTPTilesInternalsMessageHandler::~NTPTilesInternalsMessageHandler() = default;

void NTPTilesInternalsMessageHandler::RegisterMessages(
    NTPTilesInternalsMessageHandlerClient* client) {
  client_ = client;

  client_->RegisterMessageCallback(
      "registerForEvents",
      base::BindRepeating(
          &NTPTilesInternalsMessageHandler::HandleRegisterForEvents,
          base::Unretained(this)));

  client_->RegisterMessageCallback(
      "update",
      base::BindRepeating(&NTPTilesInternalsMessageHandler::HandleUpdate,
                          base::Unretained(this)));

  client_->RegisterMessageCallback(
      "fetchSuggestions",
      base::BindRepeating(
          &NTPTilesInternalsMessageHandler::HandleFetchSuggestions,
          base::Unretained(this)));

  client_->RegisterMessageCallback(
      "viewPopularSitesJson",
      base::BindRepeating(
          &NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson,
          base::Unretained(this)));
}

void NTPTilesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  if (!client_->SupportsNTPTiles()) {
    base::DictionaryValue disabled;
    disabled.SetBoolean("topSites", false);
    disabled.SetBoolean("suggestionsService", false);
    disabled.SetBoolean("popular", false);
    disabled.SetBoolean("customLinks", false);
    disabled.SetBoolean("whitelist", false);
    client_->CallJavascriptFunction(
        "chrome.ntp_tiles_internals.receiveSourceInfo", disabled);
    SendTiles(NTPTilesVector(), FaviconResultMap());
    return;
  }
  DCHECK(args->empty());

  suggestions_status_.clear();
  popular_sites_json_.clear();
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->SetMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleUpdate(
    const base::ListValue* args) {
  if (!client_->SupportsNTPTiles()) {
    return;
  }

  const base::DictionaryValue* dict = nullptr;
  DCHECK_EQ(1u, args->GetSize());
  args->GetDictionary(0, &dict);
  DCHECK(dict);

  PrefService* prefs = client_->GetPrefs();

  if (most_visited_sites_->DoesSourceExist(ntp_tiles::TileSource::POPULAR)) {
    popular_sites_json_.clear();

    std::string url;
    dict->GetString("popular.overrideURL", &url);
    if (url.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideURL);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideURL,
                       url_formatter::FixupURL(url, std::string()).spec());
    }

    std::string directory;
    dict->GetString("popular.overrideDirectory", &directory);
    if (directory.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideDirectory);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideDirectory,
                       directory);
    }

    std::string country;
    dict->GetString("popular.overrideCountry", &country);
    if (country.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideCountry);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideCountry, country);
    }

    std::string version;
    dict->GetString("popular.overrideVersion", &version);
    if (version.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideVersion);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideVersion, version);
    }
  }

  // Recreate to pick up new values.
  // TODO(sfiera): refresh MostVisitedSites without re-creating it, as soon as
  // that will pick up changes to the Popular Sites overrides.
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->SetMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleFetchSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  if (!most_visited_sites_->DoesSourceExist(
          ntp_tiles::TileSource::SUGGESTIONS_SERVICE)) {
    return;
  }

  if (most_visited_sites_->suggestions()->FetchSuggestionsData()) {
    suggestions_status_ = "fetching...";
  } else {
    suggestions_status_ = "history sync is disabled, or not yet initialized";
  }
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  if (!most_visited_sites_->DoesSourceExist(ntp_tiles::TileSource::POPULAR)) {
    return;
  }

  popular_sites_json_ =
      FormatJson(*most_visited_sites_->popular_sites()->GetCachedJson());
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::SendSourceInfo() {
  PrefService* prefs = client_->GetPrefs();
  base::DictionaryValue value;

  value.SetBoolean("topSites",
                   most_visited_sites_->DoesSourceExist(TileSource::TOP_SITES));
  value.SetBoolean("customLinks", most_visited_sites_->DoesSourceExist(
                                      TileSource::CUSTOM_LINKS));
  value.SetBoolean("whitelist",
                   most_visited_sites_->DoesSourceExist(TileSource::WHITELIST));

  if (most_visited_sites_->DoesSourceExist(TileSource::SUGGESTIONS_SERVICE)) {
    value.SetString("suggestionsService.status", suggestions_status_);
  } else {
    value.SetBoolean("suggestionsService", false);
  }

  if (most_visited_sites_->DoesSourceExist(TileSource::POPULAR)) {
    auto* popular_sites = most_visited_sites_->popular_sites();
    value.SetString("popular.url", popular_sites->GetURLToFetch().spec());
    value.SetString("popular.directory", popular_sites->GetDirectoryToFetch());
    value.SetString("popular.country", popular_sites->GetCountryToFetch());
    value.SetString("popular.version", popular_sites->GetVersionToFetch());

    value.SetString(
        "popular.overrideURL",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL));
    value.SetString(
        "popular.overrideDirectory",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideDirectory));
    value.SetString(
        "popular.overrideCountry",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry));
    value.SetString(
        "popular.overrideVersion",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion));

    value.SetString("popular.json", popular_sites_json_);
  } else {
    value.SetBoolean("popular", false);
  }

  client_->CallJavascriptFunction(
      "chrome.ntp_tiles_internals.receiveSourceInfo", value);
}

void NTPTilesInternalsMessageHandler::SendTiles(
    const NTPTilesVector& tiles,
    const FaviconResultMap& result_map) {
  auto sites_list = std::make_unique<base::ListValue>();
  for (const NTPTile& tile : tiles) {
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString("title", tile.title);
    entry->SetString("url", tile.url.spec());
    entry->SetInteger("source", static_cast<int>(tile.source));
    entry->SetString("whitelistIconPath",
                     tile.whitelist_icon_path.LossyDisplayName());
    if (tile.source == TileSource::CUSTOM_LINKS) {
      entry->SetBoolean("fromMostVisited", tile.from_most_visited);
    }

    auto icon_list = std::make_unique<base::ListValue>();
    for (const auto& entry : kIconTypesAndNames) {
      auto it = result_map.find(
          FaviconResultMap::key_type(tile.url, entry.type_enum));

      if (it != result_map.end()) {
        const favicon_base::FaviconRawBitmapResult& result = it->second;
        auto icon = std::make_unique<base::DictionaryValue>();
        icon->SetString("url", result.icon_url.spec());
        icon->SetString("type", entry.type_name);
        icon->SetBoolean("onDemand", !result.fetched_because_of_page_visit);
        icon->SetInteger("width", result.pixel_size.width());
        icon->SetInteger("height", result.pixel_size.height());
        icon_list->Append(std::move(icon));
      }
    }
    entry->Set("icons", std::move(icon_list));

    sites_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("sites", std::move(sites_list));
  client_->CallJavascriptFunction("chrome.ntp_tiles_internals.receiveSites",
                                  result);
}

void NTPTilesInternalsMessageHandler::OnURLsAvailable(
    const std::map<SectionType, NTPTilesVector>& sections) {
  cancelable_task_tracker_.TryCancelAll();

  // TODO(fhorschig): Handle non-personalized tiles - https://crbug.com/753852.
  const NTPTilesVector& tiles = sections.at(SectionType::PERSONALIZED);
  if (tiles.empty()) {
    SendTiles(tiles, FaviconResultMap());
    return;
  }

  auto on_lookup_done = base::BindRepeating(
      &NTPTilesInternalsMessageHandler::OnFaviconLookupDone,
      // Unretained(this) is safe because of |cancelable_task_tracker_|.
      base::Unretained(this), tiles, base::Owned(new FaviconResultMap()),
      base::Owned(new size_t(tiles.size() * kIconTypesAndNames.size())));

  for (const NTPTile& tile : tiles) {
    for (const auto& entry : kIconTypesAndNames) {
      favicon_service_->GetLargestRawFaviconForPageURL(
          tile.url, std::vector<favicon_base::IconTypeSet>({{entry.type_enum}}),
          /*minimum_size_in_pixels=*/0, base::Bind(on_lookup_done, tile.url),
          &cancelable_task_tracker_);
    }
  }
}

void NTPTilesInternalsMessageHandler::OnIconMadeAvailable(
    const GURL& site_url) {}

void NTPTilesInternalsMessageHandler::OnFaviconLookupDone(
    const NTPTilesVector& tiles,
    FaviconResultMap* result_map,
    size_t* num_pending_lookups,
    const GURL& page_url,
    const favicon_base::FaviconRawBitmapResult& result) {
  DCHECK_NE(0u, *num_pending_lookups);

  result_map->emplace(
      std::pair<GURL, favicon_base::IconType>(page_url, result.icon_type),
      result);

  --*num_pending_lookups;
  if (*num_pending_lookups == 0)
    SendTiles(tiles, *result_map);
}

}  // namespace ntp_tiles
