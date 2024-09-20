// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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

std::string FormatJson(const base::Value::List& value) {
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
      "viewPopularSitesJson",
      base::BindRepeating(
          &NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson,
          base::Unretained(this)));
}

void NTPTilesInternalsMessageHandler::HandleRegisterForEvents(
    const base::Value::List& args) {
  if (!client_->SupportsNTPTiles()) {
    base::Value::Dict disabled;
    disabled.Set("topSites", false);
    disabled.Set("popular", false);
    disabled.Set("customLinks", false);
    client_->CallJavascriptFunction("cr.webUIListenerCallback",
                                    base::Value("receive-source-info"),
                                    base::Value(std::move(disabled)));
    SendTiles(NTPTilesVector(), FaviconResultMap());
    return;
  }
  DCHECK_EQ(0u, args.size());

  popular_sites_json_.clear();
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->AddMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleUpdate(
    const base::Value::List& args) {
  if (!client_->SupportsNTPTiles()) {
    return;
  }

  DCHECK_EQ(1u, args.size());
  const base::Value& value = args[0];
  DCHECK(value.is_dict());
  const base::Value::Dict& dict = value.GetDict();

  PrefService* prefs = client_->GetPrefs();

  if (most_visited_sites_ &&
      most_visited_sites_->DoesSourceExist(ntp_tiles::TileSource::POPULAR)) {
    popular_sites_json_.clear();

    const std::string* url = dict.FindStringByDottedPath("popular.overrideURL");
    if (url->empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideURL);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideURL,
                       url_formatter::FixupURL(*url, std::string()).spec());
    }

    const std::string* directory =
        dict.FindStringByDottedPath("popular.overrideDirectory");
    if (directory->empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideDirectory);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideDirectory,
                       *directory);
    }

    const std::string* country =
        dict.FindStringByDottedPath("popular.overrideCountry");
    if (country->empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideCountry);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideCountry,
                       *country);
    }

    const std::string* version =
        dict.FindStringByDottedPath("popular.overrideVersion");
    if (version->empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideVersion);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideVersion,
                       *version);
    }
  }

  // Recreate to pick up new values.
  // TODO(sfiera): refresh MostVisitedSites without re-creating it, as soon as
  // that will pick up changes to the Popular Sites overrides.
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->AddMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson(
    const base::Value::List& args) {
  DCHECK_EQ(0u, args.size());
  if (!most_visited_sites_ ||
      !most_visited_sites_->DoesSourceExist(ntp_tiles::TileSource::POPULAR)) {
    return;
  }

  popular_sites_json_ =
      FormatJson(most_visited_sites_->popular_sites()->GetCachedJson());
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::SendSourceInfo() {
  PrefService* prefs = client_->GetPrefs();
  base::Value::Dict value;

  value.Set("topSites",
            most_visited_sites_->DoesSourceExist(TileSource::TOP_SITES));
  value.Set("customLinks",
            most_visited_sites_->DoesSourceExist(TileSource::CUSTOM_LINKS));

  if (most_visited_sites_->DoesSourceExist(TileSource::POPULAR)) {
    auto* popular_sites = most_visited_sites_->popular_sites();
    value.Set("popular.url", popular_sites->GetURLToFetch().spec());
    value.Set("popular.directory", popular_sites->GetDirectoryToFetch());
    value.Set("popular.country", popular_sites->GetCountryToFetch());
    value.Set("popular.version", popular_sites->GetVersionToFetch());

    value.Set("popular.overrideURL",
              prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL));
    value.Set(
        "popular.overrideDirectory",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideDirectory));
    value.Set("popular.overrideCountry",
              prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry));
    value.Set("popular.overrideVersion",
              prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion));

    value.Set("popular.json", popular_sites_json_);
  } else {
    value.Set("popular", false);
  }

  client_->CallJavascriptFunction("cr.webUIListenerCallback",
                                  base::Value("receive-source-info"),
                                  base::Value(std::move(value)));
}

void NTPTilesInternalsMessageHandler::SendTiles(
    const NTPTilesVector& tiles,
    const FaviconResultMap& result_map) {
  base::Value::List sites_list;
  for (const NTPTile& tile : tiles) {
    base::Value::Dict entry;
    entry.Set("title", tile.title);
    entry.Set("url", tile.url.spec());
    entry.Set("source", static_cast<int>(tile.source));
    entry.Set("visitCount", tile.visit_count);
    entry.Set("lastVisitTime", base::TimeFormatHTTP(tile.last_visit_time));
    entry.Set("score", tile.score);
    if (tile.source == TileSource::CUSTOM_LINKS) {
      entry.Set("fromMostVisited", tile.from_most_visited);
    }

    base::Value::List icon_list;
    for (const auto& type_and_name : kIconTypesAndNames) {
      auto it = result_map.find(
          FaviconResultMap::key_type(tile.url, type_and_name.type_enum));

      if (it != result_map.end()) {
        const favicon_base::FaviconRawBitmapResult& result = it->second;
        base::Value::Dict icon;
        icon.Set("url", result.icon_url.spec());
        icon.Set("type", type_and_name.type_name);
        icon.Set("onDemand", !result.fetched_because_of_page_visit);
        icon.Set("width", result.pixel_size.width());
        icon.Set("height", result.pixel_size.height());
        icon_list.Append(std::move(icon));
      }
    }
    entry.Set("icons", std::move(icon_list));

    sites_list.Append(std::move(entry));
  }

  base::Value::Dict result;
  result.Set("sites", std::move(sites_list));
  client_->CallJavascriptFunction("cr.webUIListenerCallback",
                                  base::Value("receive-sites"),
                                  base::Value(std::move(result)));
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
          /*minimum_size_in_pixels=*/0,
          base::BindOnce(on_lookup_done, tile.url), &cancelable_task_tracker_);
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
  if (*num_pending_lookups == 0) {
    SendTiles(tiles, *result_map);
  }
}

}  // namespace ntp_tiles
