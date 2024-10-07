// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/deleted_tile_type.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/webapps/common/constants.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/re2/src/re2/re2.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// GN doesn't understand conditional includes, so we need nogncheck here.
#include "extensions/common/constants.h"  // nogncheck
#endif

using history::TopSites;

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

bool HasHomeTile(const NTPTilesVector& tiles) {
  for (const auto& tile : tiles) {
    if (tile.source == TileSource::HOMEPAGE) {
      return true;
    }
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
std::u16string GenerateShortTitle(const std::u16string& title) {
  // Empty title only happened in the unittests.
  if (title.empty()) {
    return std::u16string();
  }

  // Match "anything- anything" where "-" is one of the delimiters shown in the
  // following examples of intended matches: "Front - Back", "Front | Back",
  // "Front: Back", "Front; Back"
  const std::string regex = "(.*?)[-|:;]+\\s(.*)";

  std::string utf8_short_title_front;
  std::string utf8_short_title_back;
  std::string utf8_title = base::UTF16ToUTF8(title);

  std::u16string short_title_front;
  std::u16string short_title_back;
  std::u16string short_title;

  if (!re2::RE2::FullMatch(utf8_title, regex, &utf8_short_title_front,
                           &utf8_short_title_back)) {
    // If FullMatch() returns false, we don't have a split title, so return full
    // title. Tests expect trimmed title.
    return std::u16string(
        base::TrimWhitespace(title, base::TrimPositions::TRIM_ALL));
  }

  if (!utf8_short_title_front.empty()) {
    short_title_front = base::UTF8ToUTF16(utf8_short_title_front);
    short_title = short_title_front;
  }
  if (!utf8_short_title_back.empty()) {
    short_title_back = base::UTF8ToUTF16(utf8_short_title_back);
  }

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
  base::TrimWhitespace(short_title, base::TrimPositions::TRIM_ALL,
                       &short_title);
  return short_title;
}

}  // namespace

MostVisitedSites::MostVisitedSites(
    PrefService* prefs,
    signin::IdentityManager* identity_manager,
    supervised_user::SupervisedUserService* supervised_user_service,
    scoped_refptr<history::TopSites> top_sites,
    std::unique_ptr<PopularSites> popular_sites,
    std::unique_ptr<CustomLinksManager> custom_links,
    std::unique_ptr<IconCacher> icon_cacher,
    bool is_default_chrome_app_migrated)
    : prefs_(prefs),
      identity_manager_(identity_manager),
      supervised_user_service_(supervised_user_service),
      top_sites_(top_sites),
      popular_sites_(std::move(popular_sites)),
      custom_links_(std::move(custom_links)),
      icon_cacher_(std::move(icon_cacher)),
      is_default_chrome_app_migrated_(is_default_chrome_app_migrated),
      max_num_sites_(0u),
      mv_source_(TileSource::TOP_SITES),
      is_observing_(false) {
  DCHECK(prefs_);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (supervised_user_service_) {
    supervised_user_service_observation_.Observe(supervised_user_service_);
  }
#endif
}

MostVisitedSites::~MostVisitedSites() {
  observers_.Clear();
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
    case TileSource::POPULAR_BAKED_IN:
    case TileSource::POPULAR:
      return popular_sites_ != nullptr;
    case TileSource::HOMEPAGE:
      return homepage_client_ != nullptr;
    case TileSource::ALLOWLIST:
      return supervised_user_service_ != nullptr;
    case TileSource::CUSTOM_LINKS:
      return custom_links_ != nullptr;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void MostVisitedSites::SetHomepageClient(
    std::unique_ptr<HomepageClient> client) {
  DCHECK(client);
  homepage_client_ = std::move(client);
}

void MostVisitedSites::AddMostVisitedURLsObserver(Observer* observer,
                                                  size_t max_num_sites) {
  observers_.AddObserver(observer);

  // All observer must provide the same |max_num_sites| value.
  DCHECK(max_num_sites_ == 0u || max_num_sites_ == max_num_sites);
  max_num_sites_ = max_num_sites;

  // Starts observing the following sources when the first observer is added.
  if (!is_observing_) {
    is_observing_ = true;
    // The order for this condition is important, ShouldShowPopularSites()
    // should always be called last to keep metrics as relevant as possible.
    if (popular_sites_ && NeedPopularSites(prefs_, GetMaxNumSites()) &&
        ShouldShowPopularSites()) {
      popular_sites_->MaybeStartFetch(
          false, base::BindOnce(&MostVisitedSites::OnPopularSitesDownloaded,
                                base::Unretained(this)));
    }

    if (top_sites_) {
      // Register as TopSitesObserver so that we can update ourselves when the
      // TopSites changes.
      top_sites_observation_.Observe(top_sites_.get());
    }

    if (custom_links_) {
      custom_links_subscription_ =
          custom_links_->RegisterCallbackForOnChanged(base::BindRepeating(
              &MostVisitedSites::OnCustomLinksChanged, base::Unretained(this)));
    }
  }

  // Immediately build the current set of tiles, getting suggestions from
  // TopSites.
  BuildCurrentTiles();
  // Also start a request for fresh suggestions.
  Refresh();
}

void MostVisitedSites::RemoveMostVisitedURLsObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MostVisitedSites::Refresh() {
  if (top_sites_) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites_->SyncWithHistory();
  }
}

void MostVisitedSites::RefreshTiles() {
  BuildCurrentTiles();
}

void MostVisitedSites::InitializeCustomLinks() {
  if (!custom_links_ || !current_tiles_.has_value() ||
      !IsCustomLinksEnabled()) {
    return;
  }

  if (custom_links_->Initialize(current_tiles_.value())) {
    custom_links_action_count_ = 0;
  }
}

void MostVisitedSites::UninitializeCustomLinks() {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return;
  }

  custom_links_action_count_ = -1;
  custom_links_->Uninitialize();
  BuildCurrentTiles();
}

bool MostVisitedSites::IsCustomLinksInitialized() {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return false;
  }

  return custom_links_->IsInitialized();
}

void MostVisitedSites::EnableCustomLinks(bool enable) {
  if (is_custom_links_enabled_ != enable) {
    is_custom_links_enabled_ = enable;
    BuildCurrentTiles();
  }
}

bool MostVisitedSites::IsCustomLinksEnabled() const {
  return is_custom_links_enabled_;
}

void MostVisitedSites::SetShortcutsVisible(bool visible) {
  if (is_shortcuts_visible_ != visible) {
    is_shortcuts_visible_ = visible;
    BuildCurrentTiles();
  }
}

bool MostVisitedSites::IsShortcutsVisible() const {
  return is_shortcuts_visible_;
}

bool MostVisitedSites::AddCustomLink(const GURL& url,
                                     const std::u16string& title) {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return false;
  }

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->AddLink(url, title);
  if (success) {
    if (custom_links_action_count_ != -1) {
      custom_links_action_count_++;
    }
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
                                        const std::u16string& new_title) {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return false;
  }

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->UpdateLink(url, new_url, new_title);
  if (success) {
    if (custom_links_action_count_ != -1) {
      custom_links_action_count_++;
    }
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

bool MostVisitedSites::ReorderCustomLink(const GURL& url, size_t new_pos) {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return false;
  }

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->ReorderLink(url, new_pos);
  if (success) {
    if (custom_links_action_count_ != -1) {
      custom_links_action_count_++;
    }
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

bool MostVisitedSites::DeleteCustomLink(const GURL& url) {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return false;
  }

  bool is_first_action = !custom_links_->IsInitialized();
  // Initialize custom links if they have not been initialized yet.
  InitializeCustomLinks();

  bool success = custom_links_->DeleteLink(url);
  if (success) {
    if (custom_links_action_count_ != -1) {
      custom_links_action_count_++;
    }
    BuildCurrentTiles();
  } else if (is_first_action) {
    // We don't want to keep custom links initialized if the first action after
    // initialization failed.
    UninitializeCustomLinks();
  }
  return success;
}

void MostVisitedSites::UndoCustomLinkAction() {
  if (!custom_links_ || !IsCustomLinksEnabled()) {
    return;
  }

  // If this is undoing the first action after initialization, uninitialize
  // custom links.
  if (custom_links_action_count_-- == 1) {
    UninitializeCustomLinks();
  } else if (custom_links_->UndoAction()) {
    BuildCurrentTiles();
  }
}

size_t MostVisitedSites::GetCustomLinkNum() {
  return custom_links_->GetLinks().size();
}

void MostVisitedSites::AddOrRemoveBlockedUrl(const GURL& url, bool add_url) {
  if (add_url) {
    base::RecordAction(base::UserMetricsAction("Suggestions.Site.Removed"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Suggestions.Site.RemovalUndone"));
  }

  if (top_sites_) {
    if (add_url) {
      top_sites_->AddBlockedUrl(url);
    } else {
      top_sites_->RemoveBlockedUrl(url);
    }
  }
}

void MostVisitedSites::ClearBlockedUrls() {
  if (top_sites_) {
    top_sites_->ClearBlockedUrls();
  }
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
void MostVisitedSites::OnURLFilterChanged() {
  BuildCurrentTiles();
}
#endif

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNumPersonalTiles, 0);
}

// static
void MostVisitedSites::ResetProfilePrefs(PrefService* prefs) {
  prefs->SetInteger(prefs::kNumPersonalTiles, 0);
}

size_t MostVisitedSites::GetMaxNumSites() const {
  return max_num_sites_ + (custom_links_ && IsCustomLinksEnabled() ? 1 : 0);
}

void MostVisitedSites::InitiateTopSitesQuery() {
  if (!top_sites_) {
    return;
  }
  if (top_sites_weak_ptr_factory_.HasWeakPtrs()) {
    return;  // Ongoing query.
  }
  top_sites_->GetMostVisitedURLs(
      base::BindOnce(&MostVisitedSites::OnMostVisitedURLsAvailable,
                     top_sites_weak_ptr_factory_.GetWeakPtr()));
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  // Ignore the event if tiles are provided by custom links, which take
  // precedence.
  if (IsCustomLinksInitialized()) {
    return;
  }

  NTPTilesVector tiles;
  size_t num_tiles = std::min(visited_list.size(), GetMaxNumSites());
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      break;  // This is the signal that there are no more real visited sites.
    }
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    if (supervised_user_service_ &&
        supervised_user_service_->IsBlockedURL(visited.url)) {
      continue;
    }
#endif

    NTPTile tile;
    tile.title =
        custom_links_ ? GenerateShortTitle(visited.title) : visited.title;
    tile.url = visited.url;
    tile.source = TileSource::TOP_SITES;
    // MostVisitedURL.title is either the title or the URL which is treated
    // exactly as the title. Differentiating here is not worth the overhead.
    tile.title_source = TileTitleSource::TITLE_TAG;
    tile.visit_count = visited.visit_count;
    tile.last_visit_time = visited.last_visit_time;
    tile.score = visited.score;
    // TODO(crbug.com/41349031): Populate |data_generation_time| here in order
    // to log UMA metrics of age.
    tiles.push_back(std::move(tile));
  }

  mv_source_ = TileSource::TOP_SITES;
  InitiateNotificationForNewTiles(std::move(tiles));
}

void MostVisitedSites::BuildCurrentTiles() {
  if (IsCustomLinksInitialized()) {
    BuildCustomLinks(custom_links_->GetLinks());
    return;
  }

  mv_source_ = TileSource::TOP_SITES;
  InitiateTopSitesQuery();
}

std::map<SectionType, NTPTilesVector>
MostVisitedSites::CreatePopularSitesSections(
    const std::set<std::string>& used_hosts,
    size_t num_actual_tiles) {
  std::map<SectionType, NTPTilesVector> sections = {
      std::make_pair(SectionType::PERSONALIZED, NTPTilesVector())};
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // For child accounts popular sites tiles will not be added.
  if (identity_manager_ &&
      supervised_user::IsPrimaryAccountSubjectToParentalControls(
          identity_manager_) == signin::Tribool::kTrue) {
    return sections;
  }
#endif

  if (!popular_sites_ || !ShouldShowPopularSites()) {
    return sections;
  }

  const std::set<std::string> no_hosts;
  for (const auto& section_type_and_sites : popular_sites()->sections()) {
    SectionType type = section_type_and_sites.first;
    const PopularSites::SitesVector& sites = section_type_and_sites.second;
    if (type == SectionType::PERSONALIZED) {
      size_t num_required_tiles = GetMaxNumSites() - num_actual_tiles;
      sections[type] =
          CreatePopularSitesTiles(/*popular_sites=*/sites,
                                  /*hosts_to_skip=*/used_hosts,
                                  /*num_max_tiles=*/num_required_tiles);
    } else {
      sections[type] =
          CreatePopularSitesTiles(/*popular_sites=*/sites,
                                  /*hosts_to_skip=*/no_hosts,
                                  /*num_max_tiles=*/GetMaxNumSites());
    }
  }
  return sections;
}

NTPTilesVector MostVisitedSites::CreatePopularSitesTiles(
    const PopularSites::SitesVector& sites_vector,
    const std::set<std::string>& hosts_to_skip,
    size_t num_max_tiles) {
  // Collect non-blocked popular suggestions, skipping those already present
  // in the personal suggestions.
  NTPTilesVector popular_sites_tiles;
  for (const PopularSites::Site& popular_site : sites_vector) {
    if (popular_sites_tiles.size() >= num_max_tiles) {
      break;
    }

    // Skip blocked sites.
    if (top_sites_ && top_sites_->IsBlocked(popular_site.url)) {
      continue;
    }

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
    icon_cacher_->StartFetchPopularSites(
        popular_site,
        base::BindOnce(&MostVisitedSites::OnIconMadeAvailable,
                       base::Unretained(this), popular_site.url),
        base::BindOnce(&MostVisitedSites::OnIconMadeAvailable,
                       base::Unretained(this), popular_site.url));
  }
  return popular_sites_tiles;
}

void MostVisitedSites::OnHomepageTitleDetermined(
    NTPTilesVector tiles,
    const std::optional<std::u16string>& title) {
  if (!title.has_value()) {
    return;  // If there is no title, the most recent tile was already sent out.
  }

  MergeMostVisitedTiles(InsertHomeTile(std::move(tiles), title.value()));
}

NTPTilesVector MostVisitedSites::InsertHomeTile(
    NTPTilesVector tiles,
    const std::u16string& title) const {
  DCHECK(homepage_client_);
  DCHECK_GT(GetMaxNumSites(), 0u);

  const GURL& homepage_url = homepage_client_->GetHomepageUrl();
  NTPTilesVector new_tiles;
  bool homepage_tile_added = false;

  for (auto& tile : tiles) {
    if (new_tiles.size() >= GetMaxNumSites()) {
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
    if (new_tiles.size() >= GetMaxNumSites()) {
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

void MostVisitedSites::OnCustomLinksChanged() {
  DCHECK(custom_links_);
  if (!IsCustomLinksEnabled()) {
    return;
  }

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
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    if (supervised_user_service_ &&
        supervised_user_service_->IsBlockedURL(link.url)) {
      continue;
    }
#endif

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
    new_tiles = InsertHomeTile(std::move(new_tiles), std::u16string());
  }
  MergeMostVisitedTiles(std::move(new_tiles));
}

void MostVisitedSites::MergeMostVisitedTiles(NTPTilesVector personal_tiles) {
  std::set<std::string> used_hosts;

  size_t num_actual_tiles = 0;

  AddToHostsAndTotalCount(personal_tiles, &used_hosts, &num_actual_tiles);

  std::map<SectionType, NTPTilesVector> sections =
      CreatePopularSitesSections(used_hosts, num_actual_tiles);
  AddToHostsAndTotalCount(sections[SectionType::PERSONALIZED], &used_hosts,
                          &num_actual_tiles);

  NTPTilesVector new_tiles =
      MergeTiles(std::move(personal_tiles),
                 std::move(sections[SectionType::PERSONALIZED]));

  SaveTilesAndNotify(std::move(new_tiles), std::move(sections));
}

void MostVisitedSites::SaveTilesAndNotify(
    NTPTilesVector new_tiles,
    std::map<SectionType, NTPTilesVector> sections) {
  // TODO(crbug.com/40802205):
  // Remove this after preinstalled apps are migrated.

  NTPTilesVector fixed_tiles = is_default_chrome_app_migrated_
                                   ? RemoveInvalidPreinstallApps(new_tiles)
                                   : new_tiles;

  if (fixed_tiles.size() != new_tiles.size()) {
    metrics::RecordsMigratedDefaultAppDeleted(
        DeletedTileType::kMostVisitedSite);
  }
  if (!current_tiles_.has_value() || (*current_tiles_ != fixed_tiles)) {
    current_tiles_.emplace(std::move(fixed_tiles));

    int num_personal_tiles = 0;
    for (const auto& tile : *current_tiles_) {
      if (tile.source != TileSource::POPULAR &&
          tile.source != TileSource::POPULAR_BAKED_IN) {
        num_personal_tiles++;
      }
    }
    prefs_->SetInteger(prefs::kNumPersonalTiles, num_personal_tiles);
  }

  if (observers_.empty()) {
    return;
  }
  sections[SectionType::PERSONALIZED] = *current_tiles_;
  for (auto& observer : observers_) {
    observer.OnURLsAvailable(sections);
  }
}

// static
bool MostVisitedSites::IsNtpTileFromPreinstalledApp(GURL url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return url.is_valid() && url.SchemeIs(extensions::kExtensionScheme) &&
         extension_misc::IsPreinstalledAppId(url.host());
#else
  return false;
#endif
}

// static
bool MostVisitedSites::WasNtpAppMigratedToWebApp(PrefService* prefs, GURL url) {
  const base::Value::List& migrated_apps =
      prefs->GetList(webapps::kWebAppsMigratedPreinstalledApps);
  for (const auto& val : migrated_apps) {
    if (val.is_string() && val.GetString() == url.host()) {
      return true;
    }
  }
  return false;
}

NTPTilesVector MostVisitedSites::RemoveInvalidPreinstallApps(
    NTPTilesVector new_tiles) {
  std::erase_if(new_tiles, [this](const NTPTile& ntp_tile) {
    return MostVisitedSites::IsNtpTileFromPreinstalledApp(ntp_tile.url) &&
           MostVisitedSites::WasNtpAppMigratedToWebApp(prefs_, ntp_tile.url);
  });
  return new_tiles;
}

NTPTilesVector MostVisitedSites::MergeTiles(NTPTilesVector personal_tiles,
                                            NTPTilesVector popular_tiles) {
  NTPTilesVector merged_tiles;
  std::move(personal_tiles.begin(), personal_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(popular_tiles.begin(), popular_tiles.end(),
            std::back_inserter(merged_tiles));

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
      icon_cacher_->StartFetchPopularSites(site, base::NullCallback(),
                                           base::NullCallback());
    }
  }
}

void MostVisitedSites::OnIconMadeAvailable(const GURL& site_url) {
  for (auto& observer : observers_) {
    observer.OnIconMadeAvailable(site_url);
  }
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
  return GetMaxNumSites() > 0u &&
         homepage_client_ &&  // No platform-specific implementation - no tile.
         homepage_client_->IsHomepageTileEnabled() &&
         !homepage_client_->GetHomepageUrl().is_empty() &&
         !(top_sites_ &&
           top_sites_->IsBlocked(homepage_client_->GetHomepageUrl()));
}

void MostVisitedSites::AddToHostsAndTotalCount(const NTPTilesVector& new_tiles,
                                               std::set<std::string>* hosts,
                                               size_t* total_tile_count) const {
  for (const auto& tile : new_tiles) {
    hosts->insert(tile.url.host());
  }
  *total_tile_count += new_tiles.size();
  DCHECK_LE(*total_tile_count, GetMaxNumSites());
}

}  // namespace ntp_tiles
