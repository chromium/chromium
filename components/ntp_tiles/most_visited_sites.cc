// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_tiles {

namespace {

// URL host prefixes. Hosts with these prefixes often redirect to each other, or
// have the same content.
// Popular sites are excluded if the user has visited a page whose host only
// differs by one of these prefixes. Even if the URL does not point to the exact
// same page, the user will have a personalized suggestion that is more likely
// to be of use for them.
// A cleaner way could be checking the history for redirects but this requires
// the page to be visited on the device.
const char* kKnownGenericPagePrefixes[] = {
    "m.", "mobile.",  // Common prefixes among popular sites.
    "edition.",       // Used among news papers (CNN, Independent, ...)
    "www.",           // Usually no-www domains redirect to www or vice-versa.
    // The following entry MUST REMAIN LAST as it is prefix of every string!
    ""};  // The no-www domain matches domains on same level .

// Determine whether we need any tiles from PopularSites to fill up a grid of
// |num_tiles| tiles.
bool NeedPopularSites(const PrefService* prefs, int num_tiles) {
  return prefs->GetInteger(prefs::kNumPersonalTiles) < num_tiles;
}

bool AreURLsEquivalent(const GURL& url1, const GURL& url2) {
  return url1.host_piece() == url2.host_piece() &&
         url1.path_piece() == url2.path_piece();
}

bool HasHomeTile(const NTPTilesVector& tiles) {
  for (const auto& tile : tiles) {
    if (tile.source == TileSource::HOMEPAGE)
      return true;
  }
  return false;
}

std::string StripFirstGenericPrefix(const std::string& host) {
  for (const char* prefix : kKnownGenericPagePrefixes) {
    if (base::StartsWith(host, prefix, base::CompareCase::INSENSITIVE_ASCII)) {
      return std::string(
          base::TrimString(host, prefix, base::TrimPositions::TRIM_LEADING));
    }
  }
  return host;
}

bool ShouldShowPopularSites() {
  return base::FeatureList::IsEnabled(kUsePopularSitesSuggestions);
}

// Generate a short title for Most Visited items before they're converted to
// custom links.
base::string16 GenerateShortTitle(const base::string16& title) {
  // Empty title only happened in the unittests.
  if (title.empty())
    return base::string16();
  std::vector<base::string16> short_title_list =
      SplitString(title, base::UTF8ToUTF16("-:|;"), base::TRIM_WHITESPACE,
                  base::SPLIT_WANT_NONEMPTY);
  // Make sure it doesn't crash when the title only contains spaces.
  if (short_title_list.empty())
    return base::string16();
  base::string16 short_title_front = short_title_list.front();
  base::string16 short_title_back = short_title_list.back();
  base::string16 short_title = short_title_front;
  if (short_title_front != short_title_back) {
    int words_in_front =
        SplitString(short_title_front, base::kWhitespaceASCIIAs16,
                    base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
            .size();
    int words_in_back =
        SplitString(short_title_back, base::kWhitespaceASCIIAs16,
                    base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
            .size();
    if (words_in_front >= 3 && words_in_back >= 1 && words_in_back <= 3) {
      short_title = short_title_back;
    }
  }
  return short_title;
}

}  // namespace

MostVisitedSites::MostVisitedSites(
    PrefService* prefs,
    scoped_refptr<history::TopSites> top_sites,
    SuggestionsService* suggestions,
    std::unique_ptr<PopularSites> popular_sites,
    std::unique_ptr<CustomLinksManager> custom_links,
    std::unique_ptr<IconCacher> icon_cacher,
    std::unique_ptr<MostVisitedSitesSupervisor> supervisor)
    : prefs_(prefs),
      top_sites_(top_sites),
      suggestions_service_(suggestions),
      popular_sites_(std::move(popular_sites)),
      custom_links_(std::move(custom_links)),
      icon_cacher_(std::move(icon_cacher)),
      supervisor_(std::move(supervisor)),
      observer_(nullptr),
      max_num_sites_(0u),
      mv_source_(TileSource::TOP_SITES) {
  DCHECK(prefs_);
  // top_sites_ can be null in tests.
  // TODO(sfiera): have iOS use a dummy TopSites in its tests.
  DCHECK(suggestions_service_);
  if (supervisor_)
    supervisor_->SetObserver(this);
}

MostVisitedSites::~MostVisitedSites() {
  if (supervisor_)
    supervisor_->SetObserver(nullptr);
}

// static
bool MostVisitedSites::IsHostOrMobilePageKnown(
    const std::set<std::string>& hosts_to_skip,
    const std::string& host) {
  std::string no_prefix_host = StripFirstGenericPrefix(host);
  for (const char* prefix : kKnownGenericPagePrefixes) {
    if (hosts_to_skip.count(prefix + no_prefix_host) ||
        hosts_to_skip.count(prefix + host)) {
      return true;
    }
  }
  return false;
}

bool MostVisitedSites::DoesSourceExist(TileSource source) const {
  switch (source) {
    case TileSource::TOP_SITES:
      return top_sites_ != nullptr;
    case TileSource::SUGGESTIONS_SERVICE:
      return suggestions_service_ != nullptr;
    case TileSource::POPULAR_BAKED_IN:
    case TileSource::POPULAR:
      return popular_sites_ != nullptr;
    case TileSource::HOMEPAGE:
      return homepage_client_ != nullptr;
    case TileSource::WHITELIST:
      return supervisor_ != nullptr;
    case TileSource::CUSTOM_LINKS:
      return custom_links_ != nullptr;
    case TileSource::EXPLORE:
      return explore_sites_client_ != nullptr;
  }
  NOTREACHED();
  return false;
}

void MostVisitedSites::SetHomepageClient(
    std::unique_ptr<HomepageClient> client) {
  DCHECK(client);
  homepage_client_ = std::move(client);
}

void MostVisitedSites::SetExploreSitesClient(
    std::unique_ptr<ExploreSitesClient> client) {
  explore_sites_client_ = std::move(client);
}

void MostVisitedSites::SetMostVisitedURLsObserver(Observer* observer,
                                                  size_t num_sites) {
  DCHECK(observer);
  observer_ = observer;
  max_num_sites_ = num_sites;

  // The order for this condition is important, ShouldShowPopularSites() should
  // always be called last to keep metrics as relevant as possible.
  if (popular_sites_ && NeedPopularSites(prefs_, max_num_sites_) &&
      ShouldShowPopularSites()) {
    popular_sites_->MaybeStartFetch(
        false, base::Bind(&MostVisitedSites::OnPopularSitesDownloaded,
                          base::Unretained(this)));
  }

  if (top_sites_) {
    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    top_sites_observer_.Add(top_sites_.get());
  }

  if (custom_links_) {
    custom_links_subscription_ =
        custom_links_->RegisterCallbackForOnChanged(base::BindRepeating(
            &MostVisitedSites::OnCustomLinksChanged, base::Unretained(this)));
  }

  suggestions_subscription_ = suggestions_service_->AddCallback(base::Bind(
      &MostVisitedSites::OnSuggestionsProfileChanged, base::Unretained(this)));

  // Immediately build the current set of tiles, getting suggestions from the
  // SuggestionsService's cache or, if that is empty, sites from TopSites.
  BuildCurrentTiles();
  // Also start a request for fresh suggestions.
  Refresh();
}

void MostVisitedSites::Refresh() {
  if (top_sites_) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    // TODO(mastiz): Is seems unnecessary to refresh TopSites if we will end up
    // using server-side suggestions.
    top_sites_->SyncWithHistory();
  }

  suggestions_service_->FetchSuggestionsData();
}

void MostVisitedSites::RefreshTiles() {
  BuildCurrentTiles();
}

void MostVisitedSites::InitializeCustomLinks() {
  if (!custom_links_ || !current_tiles_.has_value() || !custom_links_enabled_)
    return;

  if (custom_links_->Initialize(current_tiles_.value()))
    custom_links_action_count_ = 0;
}

void MostVisitedSites::UninitializeCustomLinks() {
  if (!custom_links_ || !custom_links_enabled_)
    return;

  custom_links_action_count_ = -1;
  custom_links_->Uninitialize();
  BuildCurrentTiles();
}

bool MostVisitedSites::IsCustomLinksInitialized() {
  if (!custom_links_ || !custom_links_enabled_)
    return false;

  return custom_links_->IsInitialized();
}

void MostVisitedSites::EnableCustomLinks(bool enable) {
  if (custom_links_enabled_ != enable) {
    custom_links_enabled_ = enable;
    BuildCurrentTiles();
  }
}

bool MostVisitedSites::AddCustomLink(const GURL& url,
                                     const base::string16& title) {
  if (!custom_links_ || !custom_links_enabled_)
    return false;

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->AddLink(url, title);
  if (success) {
    if (custom_links_action_count_ != -1)
      custom_links_action_count_++;
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

bool MostVisitedSites::UpdateCustomLink(const GURL& url,
                                        const GURL& new_url,
                                        const base::string16& new_title) {
  if (!custom_links_ || !custom_links_enabled_)
    return false;

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->UpdateLink(url, new_url, new_title);
  if (success) {
    if (custom_links_action_count_ != -1)
      custom_links_action_count_++;
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

bool MostVisitedSites::ReorderCustomLink(const GURL& url, size_t new_pos) {
  if (!custom_links_ || !custom_links_enabled_)
    return false;

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->ReorderLink(url, new_pos);
  if (success) {
    if (custom_links_action_count_ != -1)
      custom_links_action_count_++;
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

bool MostVisitedSites::DeleteCustomLink(const GURL& url) {
  if (!custom_links_ || !custom_links_enabled_)
    return false;

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->DeleteLink(url);
  if (success) {
    if (custom_links_action_count_ != -1)
      custom_links_action_count_++;
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

void MostVisitedSites::UndoCustomLinkAction() {
  if (!custom_links_ || !custom_links_enabled_)
    return;

  // If this is undoing the first action after initialization, uninitialize
  // custom links.
  if (custom_links_action_count_-- == 1)
    UninitializeCustomLinks();
  else if (custom_links_->UndoAction())
    BuildCurrentTiles();
}

void MostVisitedSites::AddOrRemoveBlacklistedUrl(const GURL& url,
                                                 bool add_url) {
  if (add_url) {
    base::RecordAction(base::UserMetricsAction("Suggestions.Site.Removed"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Suggestions.Site.RemovalUndone"));
  }

  if (top_sites_) {
    // Always blacklist in the local TopSites.
    if (add_url)
      top_sites_->AddBlacklistedURL(url);
    else
      top_sites_->RemoveBlacklistedURL(url);
  }

  // Only blacklist in the server-side suggestions service if it's active.
  if (mv_source_ == TileSource::SUGGESTIONS_SERVICE) {
    if (add_url)
      suggestions_service_->BlacklistURL(url);
    else
      suggestions_service_->UndoBlacklistURL(url);
  }
}

void MostVisitedSites::ClearBlacklistedUrls() {
  if (top_sites_) {
    // Always update the blacklist in the local TopSites.
    top_sites_->ClearBlacklistedURLs();
  }

  // Only update the server-side blacklist if it's active.
  if (mv_source_ == TileSource::SUGGESTIONS_SERVICE) {
    suggestions_service_->ClearBlacklist();
  }
}

void MostVisitedSites::OnBlockedSitesChanged() {
  BuildCurrentTiles();
}

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNumPersonalTiles, 0);
}

void MostVisitedSites::InitiateTopSitesQuery() {
  if (!top_sites_)
    return;
  if (top_sites_weak_ptr_factory_.HasWeakPtrs())
    return;  // Ongoing query.
  top_sites_->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 top_sites_weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath MostVisitedSites::GetWhitelistLargeIconPath(const GURL& url) {
  if (supervisor_) {
    for (const auto& whitelist : supervisor_->GetWhitelists()) {
      if (AreURLsEquivalent(whitelist.entry_point, url))
        return whitelist.large_icon_path;
    }
  }
  return base::FilePath();
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  // Ignore the event if tiles are provided by the Suggestions Service or custom
  // links, which take precedence.
  if (IsCustomLinksInitialized() ||
      mv_source_ == TileSource::SUGGESTIONS_SERVICE) {
    return;
  }

  NTPTilesVector tiles;
  size_t num_tiles = std::min(visited_list.size(), max_num_sites_);
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty())
      break;  // This is the signal that there are no more real visited sites.
    if (supervisor_ && supervisor_->IsBlocked(visited.url))
      continue;

    NTPTile tile;
    tile.title =
        custom_links_ ? GenerateShortTitle(visited.title) : visited.title;
    tile.url = visited.url;
    tile.source = TileSource::TOP_SITES;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(visited.url);
    // MostVisitedURL.title is either the title or the URL which is treated
    // exactly as the title. Differentiating here is not worth the overhead.
    tile.title_source = TileTitleSource::TITLE_TAG;
    // TODO(crbug.com/773278): Populate |data_generation_time| here in order to
    // log UMA metrics of age.
    tiles.push_back(std::move(tile));
  }

  mv_source_ = TileSource::TOP_SITES;
  InitiateNotificationForNewTiles(std::move(tiles));
}

void MostVisitedSites::OnSuggestionsProfileChanged(
    const SuggestionsProfile& suggestions_profile) {
  // Ignore the event if tiles are provided by custom links, which take
  // precedence.
  if (IsCustomLinksInitialized() ||
      (suggestions_profile.suggestions_size() == 0 &&
       mv_source_ != TileSource::SUGGESTIONS_SERVICE)) {
    return;
  }

  BuildCurrentTilesGivenSuggestionsProfile(suggestions_profile);
}

void MostVisitedSites::BuildCurrentTiles() {
  if (IsCustomLinksInitialized()) {
    BuildCustomLinks(custom_links_->GetLinks());
    return;
  }

  BuildCurrentTilesGivenSuggestionsProfile(
      suggestions_service_->GetSuggestionsDataFromCache().value_or(
          SuggestionsProfile()));
}

void MostVisitedSites::BuildCurrentTilesGivenSuggestionsProfile(
    const suggestions::SuggestionsProfile& suggestions_profile) {
  size_t num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local TopSites.
  if (num_tiles == 0 ||
      !base::FeatureList::IsEnabled(kDisplaySuggestionsServiceTiles)) {
    mv_source_ = TileSource::TOP_SITES;
    InitiateTopSitesQuery();
    return;
  }
  if (max_num_sites_ < num_tiles)
    num_tiles = max_num_sites_;

  const base::Time profile_timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMicroseconds(suggestions_profile.timestamp());

  NTPTilesVector tiles;
  for (size_t i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion_pb = suggestions_profile.suggestions(i);
    GURL url(suggestion_pb.url());
    if (supervisor_ && supervisor_->IsBlocked(url))
      continue;

    NTPTile tile;
    tile.title =
        custom_links_
            ? GenerateShortTitle(base::UTF8ToUTF16(suggestion_pb.title()))
            : base::UTF8ToUTF16(suggestion_pb.title());
    tile.url = url;
    tile.source = TileSource::SUGGESTIONS_SERVICE;
    // The title is an aggregation of multiple history entries of one site.
    tile.title_source = TileTitleSource::INFERRED;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(url);
    tile.favicon_url = GURL(suggestion_pb.favicon_url());
    tile.data_generation_time = profile_timestamp;

    icon_cacher_->StartFetchMostLikely(
        url, base::BindRepeating(&MostVisitedSites::OnIconMadeAvailable,
                                 base::Unretained(this), url));

    tiles.push_back(std::move(tile));
  }

  mv_source_ = TileSource::SUGGESTIONS_SERVICE;
  InitiateNotificationForNewTiles(std::move(tiles));
}

NTPTilesVector MostVisitedSites::CreateWhitelistEntryPointTiles(
    const std::set<std::string>& used_hosts,
    size_t num_actual_tiles) {
  if (!supervisor_) {
    return NTPTilesVector();
  }

  NTPTilesVector whitelist_tiles;
  for (const auto& whitelist : supervisor_->GetWhitelists()) {
    if (whitelist_tiles.size() + num_actual_tiles >= max_num_sites_)
      break;

    // Skip blacklisted sites.
    if (top_sites_ && top_sites_->IsBlacklisted(whitelist.entry_point))
      continue;

    // Skip tiles already present.
    if (used_hosts.find(whitelist.entry_point.host()) != used_hosts.end())
      continue;

    // Skip whitelist entry points that are manually blocked.
    if (supervisor_->IsBlocked(whitelist.entry_point))
      continue;

    NTPTile tile;
    tile.title = whitelist.title;
    tile.url = whitelist.entry_point;
    tile.source = TileSource::WHITELIST;
    // User-set. Might be the title but we cannot be sure.
    tile.title_source = TileTitleSource::UNKNOWN;
    tile.whitelist_icon_path = whitelist.large_icon_path;
    whitelist_tiles.push_back(std::move(tile));
  }

  return whitelist_tiles;
}

std::map<SectionType, NTPTilesVector>
MostVisitedSites::CreatePopularSitesSections(
    const std::set<std::string>& used_hosts,
    size_t num_actual_tiles) {
  std::map<SectionType, NTPTilesVector> sections = {
      std::make_pair(SectionType::PERSONALIZED, NTPTilesVector())};
  // For child accounts popular sites tiles will not be added.
  if (supervisor_ && supervisor_->IsChildProfile()) {
    return sections;
  }

  if (!popular_sites_ || !ShouldShowPopularSites()) {
    return sections;
  }

  const std::set<std::string> no_hosts;
  for (const auto& section_type_and_sites : popular_sites()->sections()) {
    SectionType type = section_type_and_sites.first;
    const PopularSites::SitesVector& sites = section_type_and_sites.second;
    if (type == SectionType::PERSONALIZED) {
      size_t num_required_tiles = max_num_sites_ - num_actual_tiles;
      sections[type] =
          CreatePopularSitesTiles(/*popular_sites=*/sites,
                                  /*hosts_to_skip=*/used_hosts,
                                  /*num_max_tiles=*/num_required_tiles);
    } else {
      sections[type] =
          CreatePopularSitesTiles(/*popular_sites=*/sites,
                                  /*hosts_to_skip=*/no_hosts,
                                  /*num_max_tiles=*/max_num_sites_);
    }
  }
  return sections;
}

NTPTilesVector MostVisitedSites::CreatePopularSitesTiles(
    const PopularSites::SitesVector& sites_vector,
    const std::set<std::string>& hosts_to_skip,
    size_t num_max_tiles) {
  // Collect non-blacklisted popular suggestions, skipping those already present
  // in the personal suggestions.
  NTPTilesVector popular_sites_tiles;
  for (const PopularSites::Site& popular_site : sites_vector) {
    if (popular_sites_tiles.size() >= num_max_tiles) {
      break;
    }

    // Skip blacklisted sites.
    if (top_sites_ && top_sites_->IsBlacklisted(popular_site.url))
      continue;

    const std::string& host = popular_site.url.host();
    if (IsHostOrMobilePageKnown(hosts_to_skip, host)) {
      continue;
    }

    NTPTile tile;
    tile.title = popular_site.title;
    tile.url = GURL(popular_site.url);
    tile.title_source = popular_site.title_source;
    tile.source = popular_site.baked_in ? TileSource::POPULAR_BAKED_IN
                                        : TileSource::POPULAR;
    popular_sites_tiles.push_back(std::move(tile));
    base::Closure icon_available =
        base::Bind(&MostVisitedSites::OnIconMadeAvailable,
                   base::Unretained(this), popular_site.url);
    icon_cacher_->StartFetchPopularSites(popular_site, icon_available,
                                         icon_available);
  }
  return popular_sites_tiles;
}

void MostVisitedSites::OnHomepageTitleDetermined(
    NTPTilesVector tiles,
    const base::Optional<base::string16>& title) {
  if (!title.has_value())
    return;  // If there is no title, the most recent tile was already sent out.

  MergeMostVisitedTiles(InsertHomeTile(std::move(tiles), title.value()));
}

NTPTilesVector MostVisitedSites::InsertHomeTile(
    NTPTilesVector tiles,
    const base::string16& title) const {
  DCHECK(homepage_client_);
  DCHECK_GT(max_num_sites_, 0u);

  const GURL& homepage_url = homepage_client_->GetHomepageUrl();
  NTPTilesVector new_tiles;
  bool homepage_tile_added = false;

  for (auto& tile : tiles) {
    if (new_tiles.size() >= max_num_sites_) {
      break;
    }

    // If there's a tile has the same host name with homepage, insert the tile
    // to the first position of the list. This is also a deduplication.
    if (tile.url.host() == homepage_url.host() && !homepage_tile_added) {
      tile.source = TileSource::HOMEPAGE;
      homepage_tile_added = true;
      new_tiles.insert(new_tiles.begin(), std::move(tile));
      continue;
    }
    new_tiles.push_back(std::move(tile));
  }

  if (!homepage_tile_added) {
    // Make room for the homepage tile.
    if (new_tiles.size() >= max_num_sites_) {
      new_tiles.pop_back();
    }
    NTPTile homepage_tile;
    homepage_tile.url = homepage_url;
    homepage_tile.title = title;
    homepage_tile.source = TileSource::HOMEPAGE;
    homepage_tile.title_source = TileTitleSource::TITLE_TAG;

    // Always insert |homepage_tile| to the front of |new_tiles| to ensure it's
    // the first tile.
    new_tiles.insert(new_tiles.begin(), std::move(homepage_tile));
  }
  return new_tiles;
}

base::Optional<NTPTile> MostVisitedSites::CreateExploreSitesTile() {
  if (!explore_sites_client_)
    return base::nullopt;

  NTPTile explore_sites_tile;
  explore_sites_tile.url = explore_sites_client_->GetExploreSitesUrl();
  explore_sites_tile.title = explore_sites_client_->GetExploreSitesTitle();
  explore_sites_tile.source = TileSource::EXPLORE;
  explore_sites_tile.title_source = TileTitleSource::UNKNOWN;

  return explore_sites_tile;
}

void MostVisitedSites::OnCustomLinksChanged() {
  DCHECK(custom_links_);
  if (!custom_links_enabled_)
    return;

  if (custom_links_->IsInitialized()) {
    BuildCustomLinks(custom_links_->GetLinks());
  } else {
    // Since custom links have been uninitialized (e.g. through Chrome sync), we
    // should show the regular Most Visited tiles.
    BuildCurrentTiles();
  }
}

void MostVisitedSites::BuildCustomLinks(
    const std::vector<CustomLinksManager::Link>& links) {
  DCHECK(custom_links_);

  NTPTilesVector tiles;
  // The maximum number of custom links that can be shown is independent of the
  // maximum number of Most Visited sites that can be shown.
  size_t num_tiles = std::min(links.size(), kMaxNumCustomLinks);
  for (size_t i = 0; i < num_tiles; ++i) {
    const CustomLinksManager::Link& link = links.at(i);
    if (supervisor_ && supervisor_->IsBlocked(link.url))
      continue;

    NTPTile tile;
    tile.title = link.title;
    tile.url = link.url;
    tile.source = TileSource::CUSTOM_LINKS;
    tile.from_most_visited = link.is_most_visited;
    tiles.push_back(std::move(tile));
  }

  mv_source_ = TileSource::CUSTOM_LINKS;
  SaveTilesAndNotify(std::move(tiles), std::map<SectionType, NTPTilesVector>());
}

void MostVisitedSites::InitiateNotificationForNewTiles(
    NTPTilesVector new_tiles) {
  if (ShouldAddHomeTile() && !HasHomeTile(new_tiles)) {
    homepage_client_->QueryHomepageTitle(
        base::BindOnce(&MostVisitedSites::OnHomepageTitleDetermined,
                       base::Unretained(this), new_tiles));
    GURL homepage_url = homepage_client_->GetHomepageUrl();
    icon_cacher_->StartFetchMostLikely(
        homepage_url,
        base::BindRepeating(&MostVisitedSites::OnIconMadeAvailable,
                            base::Unretained(this), homepage_url));

    // Don't wait for the homepage title from history but immediately serve a
    // copy of new tiles.
    new_tiles = InsertHomeTile(std::move(new_tiles), base::string16());
  }
  MergeMostVisitedTiles(std::move(new_tiles));
}

void MostVisitedSites::MergeMostVisitedTiles(NTPTilesVector personal_tiles) {
  std::set<std::string> used_hosts;

  base::Optional<NTPTile> explore_tile = CreateExploreSitesTile();
  size_t num_actual_tiles = explore_tile ? 1 : 0;

  // The explore sites tile may have taken a space that was utilized by the
  // personal tiles.
  if (personal_tiles.size() + num_actual_tiles > max_num_sites_) {
    personal_tiles.pop_back();
  }
  AddToHostsAndTotalCount(personal_tiles, &used_hosts, &num_actual_tiles);

  NTPTilesVector whitelist_tiles =
      CreateWhitelistEntryPointTiles(used_hosts, num_actual_tiles);
  AddToHostsAndTotalCount(whitelist_tiles, &used_hosts, &num_actual_tiles);

  std::map<SectionType, NTPTilesVector> sections =
      CreatePopularSitesSections(used_hosts, num_actual_tiles);
  AddToHostsAndTotalCount(sections[SectionType::PERSONALIZED], &used_hosts,
                          &num_actual_tiles);

  NTPTilesVector new_tiles =
      MergeTiles(std::move(personal_tiles), std::move(whitelist_tiles),
                 std::move(sections[SectionType::PERSONALIZED]), explore_tile);

  SaveTilesAndNotify(std::move(new_tiles), std::move(sections));
}

void MostVisitedSites::SaveTilesAndNotify(
    NTPTilesVector new_tiles,
    std::map<SectionType, NTPTilesVector> sections) {
  if (current_tiles_.has_value() && (*current_tiles_ == new_tiles))
    return;
  current_tiles_.emplace(std::move(new_tiles));

  int num_personal_tiles = 0;
  for (const auto& tile : *current_tiles_) {
    if (tile.source != TileSource::POPULAR &&
        tile.source != TileSource::POPULAR_BAKED_IN) {
      num_personal_tiles++;
    }
  }
  prefs_->SetInteger(prefs::kNumPersonalTiles, num_personal_tiles);
  if (!observer_)
    return;
  sections[SectionType::PERSONALIZED] = *current_tiles_;
  observer_->OnURLsAvailable(sections);
}

// static
NTPTilesVector MostVisitedSites::MergeTiles(
    NTPTilesVector personal_tiles,
    NTPTilesVector whitelist_tiles,
    NTPTilesVector popular_tiles,
    base::Optional<NTPTile> explore_tile) {
  NTPTilesVector merged_tiles;
  std::move(personal_tiles.begin(), personal_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(whitelist_tiles.begin(), whitelist_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(popular_tiles.begin(), popular_tiles.end(),
            std::back_inserter(merged_tiles));
  if (explore_tile)
    merged_tiles.push_back(*explore_tile);

  return merged_tiles;
}

void MostVisitedSites::OnPopularSitesDownloaded(bool success) {
  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  for (const auto& section : popular_sites_->sections()) {
    for (const PopularSites::Site& site : section.second) {
      // Ignore callback; these icons will be seen on the *next* NTP.
      icon_cacher_->StartFetchPopularSites(site, base::Closure(),
                                           base::Closure());
    }
  }
}

void MostVisitedSites::OnIconMadeAvailable(const GURL& site_url) {
  observer_->OnIconMadeAvailable(site_url);
}

void MostVisitedSites::TopSitesLoaded(TopSites* top_sites) {}

void MostVisitedSites::TopSitesChanged(TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == TileSource::TOP_SITES) {
    // The displayed tiles are invalidated.
    InitiateTopSitesQuery();
  }
}

bool MostVisitedSites::ShouldAddHomeTile() const {
  return max_num_sites_ > 0u &&
         homepage_client_ &&  // No platform-specific implementation - no tile.
         homepage_client_->IsHomepageTileEnabled() &&
         !homepage_client_->GetHomepageUrl().is_empty() &&
         !(top_sites_ &&
           top_sites_->IsBlacklisted(homepage_client_->GetHomepageUrl()));
}

void MostVisitedSites::AddToHostsAndTotalCount(const NTPTilesVector& new_tiles,
                                               std::set<std::string>* hosts,
                                               size_t* total_tile_count) const {
  for (const auto& tile : new_tiles) {
    hosts->insert(tile.url.host());
  }
  *total_tile_count += new_tiles.size();
  DCHECK_LE(*total_tile_count, max_num_sites_);
}

}  // namespace ntp_tiles
