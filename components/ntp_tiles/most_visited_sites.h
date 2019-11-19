// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
#define COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/ntp_tiles/section_type.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "url/gurl.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace ntp_tiles {

class IconCacher;

// Shim interface for SupervisedUserService.
class MostVisitedSitesSupervisor {
 public:
  struct Whitelist {
    base::string16 title;
    GURL entry_point;
    base::FilePath large_icon_path;
  };

  class Observer {
   public:
    virtual void OnBlockedSitesChanged() = 0;

   protected:
    ~Observer() {}
  };

  virtual ~MostVisitedSitesSupervisor() {}

  // Pass non-null to set observer, or null to remove observer.
  // If setting observer, there must not yet be an observer set.
  // If removing observer, there must already be one to remove.
  // Does not take ownership. Observer must outlive this object.
  virtual void SetObserver(Observer* new_observer) = 0;

  // If true, |url| should not be shown on the NTP.
  virtual bool IsBlocked(const GURL& url) = 0;

  // Explicitly-specified sites to show on NTP.
  virtual std::vector<Whitelist> GetWhitelists() = 0;

  // If true, be conservative about suggesting sites from outside sources.
  virtual bool IsChildProfile() = 0;
};

// Tracks the list of most visited sites.
class MostVisitedSites : public history::TopSitesObserver,
                         public MostVisitedSitesSupervisor::Observer {
 public:
  // The observer to be notified when the list of most visited sites changes.
  class Observer {
   public:
    // |sections| must at least contain the PERSONALIZED section.
    virtual void OnURLsAvailable(
        const std::map<SectionType, NTPTilesVector>& sections) = 0;
    virtual void OnIconMadeAvailable(const GURL& site_url) = 0;

   protected:
    virtual ~Observer() {}
  };

  // This interface delegates the retrieval of the homepage to the
  // platform-specific implementation.
  class HomepageClient {
   public:
    using TitleCallback =
        base::OnceCallback<void(const base::Optional<base::string16>& title)>;

    virtual ~HomepageClient() = default;
    virtual bool IsHomepageTileEnabled() const = 0;
    virtual GURL GetHomepageUrl() const = 0;
    // TODO(https://crbug.com/862753): Extract this to another interface.
    virtual void QueryHomepageTitle(TitleCallback title_callback) = 0;
  };

  class ExploreSitesClient {
   public:
    virtual ~ExploreSitesClient() = default;
    virtual GURL GetExploreSitesUrl() const = 0;
    virtual base::string16 GetExploreSitesTitle() const = 0;
  };

  // Construct a MostVisitedSites instance.
  //
  // |prefs| and |suggestions| are required and may not be null. |top_sites|,
  // |popular_sites|, |custom_links|, |supervisor| and |homepage_client| are
  //  optional and if null, the associated features will be disabled.
  MostVisitedSites(PrefService* prefs,
                   scoped_refptr<history::TopSites> top_sites,
                   suggestions::SuggestionsService* suggestions,
                   std::unique_ptr<PopularSites> popular_sites,
                   std::unique_ptr<CustomLinksManager> custom_links,
                   std::unique_ptr<IconCacher> icon_cacher,
                   std::unique_ptr<MostVisitedSitesSupervisor> supervisor);

  ~MostVisitedSites() override;

  // Returns true if this object was created with a non-null provider for the
  // given NTP tile source. That source may or may not actually provide tiles,
  // depending on its configuration and the priority of different sources.
  bool DoesSourceExist(TileSource source) const;

  // Returns the corresponding object passed at construction.
  history::TopSites* top_sites() { return top_sites_.get(); }
  suggestions::SuggestionsService* suggestions() {
    return suggestions_service_;
  }
  PopularSites* popular_sites() { return popular_sites_.get(); }
  MostVisitedSitesSupervisor* supervisor() { return supervisor_.get(); }

  // Sets the observer, and immediately fetches the current suggestions.
  // Does not take ownership of |observer|, which must outlive this object and
  // must not be null.
  void SetMostVisitedURLsObserver(Observer* observer, size_t num_sites);

  // Sets the client that provides platform-specific homepage preferences.
  // When used to replace an existing client, the new client will first be
  // used during the construction of a new tile set.
  // |client| must not be null and outlive this object.
  void SetHomepageClient(std::unique_ptr<HomepageClient> client);

  // Sets the client that provides the Explore Sites tile. Can be null if no
  // such tile is desirable.
  void SetExploreSitesClient(std::unique_ptr<ExploreSitesClient> client);

  // Requests an asynchronous refresh of the suggestions. Notifies the observer
  // if the request resulted in the set of tiles changing.
  void Refresh();

  // Forces a rebuild of the current tiles.
  void RefreshTiles();

  // Initializes custom links, which "freezes" the current MV tiles and converts
  // them to custom links. Once custom links is initialized, MostVisitedSites
  // will return only custom links. If the Most Visited tiles have not been
  // loaded yet, does nothing. Custom links must be enabled.
  void InitializeCustomLinks();
  // Uninitializes custom links and reverts back to regular MV tiles. The
  // current custom links will be deleted. Custom links must be enabled.
  void UninitializeCustomLinks();
  // Returns true if custom links has been initialized and not disabled, false
  // otherwise.
  bool IsCustomLinksInitialized();
  // Enables or disables custom links, but does not (un)initialize them. Called
  // when a third-party NTP is being used, or when the user switches between
  // custom links and Most Visited sites.
  void EnableCustomLinks(bool enable);
  // Adds a custom link. If the number of current links is maxed, returns false
  // and does nothing. Will initialize custom links if they have not been
  // initialized yet, unless the action fails. Custom links must be enabled.
  bool AddCustomLink(const GURL& url, const base::string16& title);
  // Updates the URL and/or title of the custom link specified by |url|. If
  // |url| does not exist or |new_url| already exists in the custom link list,
  // returns false and does nothing. Will initialize custom links if they have
  // not been initialized yet, unless the action fails. Custom links must be
  // enabled.
  bool UpdateCustomLink(const GURL& url,
                        const GURL& new_url,
                        const base::string16& new_title);
  // Moves the custom link specified by |url| to the index |new_pos|. If |url|
  // does not exist, or |new_pos| is invalid, returns false and does nothing.
  // Will initialize custom links if they have not been initialized yet, unless
  // the action fails. Custom links must be enabled.
  bool ReorderCustomLink(const GURL& url, size_t new_pos);
  // Deletes the custom link with the specified |url|. If |url| does not exist
  // in the custom link list, returns false and does nothing. Will initialize
  // custom links if they have not been initialized yet, unless the action
  // fails. Custom links must be enabled.
  bool DeleteCustomLink(const GURL& url);
  // Restores the previous state of custom links before the last action that
  // modified them. If there was no action, does nothing. If this is undoing the
  // first action after initialization, uninitializes the links. Custom links
  // must be enabled.
  void UndoCustomLinkAction();

  void AddOrRemoveBlacklistedUrl(const GURL& url, bool add_url);
  void ClearBlacklistedUrls();

  // MostVisitedSitesSupervisor::Observer implementation.
  void OnBlockedSitesChanged() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Workhorse for SaveNewTilesAndNotify. Implemented as a separate static and
  // public method for ease of testing.
  static NTPTilesVector MergeTiles(NTPTilesVector personal_tiles,
                                   NTPTilesVector whitelist_tiles,
                                   NTPTilesVector popular_tiles,
                                   base::Optional<NTPTile> explore_tile);

 private:
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesTest,
                           ShouldDeduplicateDomainWithNoWwwDomain);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesTest,
                           ShouldDeduplicateDomainByRemovingMobilePrefixes);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesTest,
                           ShouldDeduplicateDomainByReplacingMobilePrefixes);

  // This function tries to match the given |host| to a close fit in
  // |hosts_to_skip| by removing a prefix that is commonly used to redirect from
  // or to mobile pages (m.xyz.com --> xyz.com).
  // If this approach fails, the prefix is replaced by another prefix.
  // That way, true is returned for m.x.com if www.x.com is in |hosts_to_skip|.
  static bool IsHostOrMobilePageKnown(
      const std::set<std::string>& hosts_to_skip,
      const std::string& host);

  // Initialize the query to Top Sites. Called if the SuggestionsService
  // returned no data.
  void InitiateTopSitesQuery();

  // If there's a whitelist entry point for the URL, return the large icon path.
  base::FilePath GetWhitelistLargeIconPath(const GURL& url);

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& visited_list);

  // Callback for when an update is reported by the SuggestionsService.
  void OnSuggestionsProfileChanged(
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Builds the current tileset based on available caches and notifies the
  // observer.
  void BuildCurrentTiles();

  // Same as above the SuggestionsProfile is provided, no need to read cache.
  void BuildCurrentTilesGivenSuggestionsProfile(
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Creates whitelist entry point suggestions whose hosts weren't used yet.
  NTPTilesVector CreateWhitelistEntryPointTiles(
      const std::set<std::string>& used_hosts,
      size_t num_actual_tiles);

  // Creates tiles for all popular site sections. Uses |num_actual_tiles| and
  // |used_hosts| to restrict results for the PERSONALIZED section.
  std::map<SectionType, NTPTilesVector> CreatePopularSitesSections(
      const std::set<std::string>& used_hosts,
      size_t num_actual_tiles);

  // Creates tiles for |sites_vector|. The returned vector will neither contain
  // more than |num_max_tiles| nor include sites in |hosts_to_skip|.
  NTPTilesVector CreatePopularSitesTiles(
      const PopularSites::SitesVector& sites_vector,
      const std::set<std::string>& hosts_to_skip,
      size_t num_max_tiles);

  // Callback for when an update is reported by CustomLinksManager.
  void OnCustomLinksChanged();

  // Creates tiles for |links| up to |max_num_sites_|. |links| will never exceed
  // a certain maximum.
  void BuildCustomLinks(const std::vector<CustomLinksManager::Link>& links);

  // Initiates a query for the homepage tile if needed and calls
  // |SaveTilesAndNotify| in the end.
  void InitiateNotificationForNewTiles(NTPTilesVector new_tiles);

  // Takes the personal tiles, creates and merges in whitelist and popular tiles
  // if appropriate. Calls |SaveTilesAndNotify| at the end.
  void MergeMostVisitedTiles(NTPTilesVector personal_tiles);

  // Saves the new tiles and notifies the observer if the tiles were actually
  // changed.
  void SaveTilesAndNotify(NTPTilesVector new_tiles,
                          std::map<SectionType, NTPTilesVector> sections);

  void OnPopularSitesDownloaded(bool success);

  void OnIconMadeAvailable(const GURL& site_url);

  // Updates the already used hosts and the total tile count based on given new
  // tiles. Enforces that the required amount of tiles is not exceeded.
  void AddToHostsAndTotalCount(const NTPTilesVector& new_tiles,
                               std::set<std::string>* hosts,
                               size_t* total_tile_count) const;

  // Adds the homepage as first tile to |tiles| and returns them as new vector.
  // Drops existing tiles with the same host as the home page and tiles that
  // would exceed the maximum.
  NTPTilesVector InsertHomeTile(NTPTilesVector tiles,
                                const base::string16& title) const;

  // Creates a tile for the Explore Sites page, if enabled. The tile is added to
  // the front of the list.
  base::Optional<NTPTile> CreateExploreSitesTile();

  void OnHomepageTitleDetermined(NTPTilesVector tiles,
                                 const base::Optional<base::string16>& title);

  // Returns true if there is a valid homepage that can be pinned as tile.
  bool ShouldAddHomeTile() const;

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  PrefService* prefs_;
  scoped_refptr<history::TopSites> top_sites_;
  suggestions::SuggestionsService* suggestions_service_;
  std::unique_ptr<PopularSites> const popular_sites_;
  std::unique_ptr<CustomLinksManager> const custom_links_;
  std::unique_ptr<IconCacher> const icon_cacher_;
  std::unique_ptr<MostVisitedSitesSupervisor> supervisor_;
  std::unique_ptr<HomepageClient> homepage_client_;
  std::unique_ptr<ExploreSitesClient> explore_sites_client_;

  Observer* observer_;

  // The maximum number of most visited sites to return.
  size_t max_num_sites_;

  // False if custom links is disabled and Most Visited sites should be returned
  // instead.
  bool custom_links_enabled_ = true;
  // Number of actions after custom link initialization. Set to -1 and not
  // incremented if custom links was not initialized during this session.
  int custom_links_action_count_ = -1;

  std::unique_ptr<
      suggestions::SuggestionsService::ResponseCallbackList::Subscription>
      suggestions_subscription_;

  ScopedObserver<history::TopSites, history::TopSitesObserver>
      top_sites_observer_{this};

  std::unique_ptr<base::CallbackList<void()>::Subscription>
      custom_links_subscription_;

  // The main source of personal tiles - either TOP_SITES or SUGGESTIONS_SEVICE.
  TileSource mv_source_;

  // Current set of tiles. Optional so that the observer can be notified
  // whenever it changes, including possibily an initial change from
  // !current_tiles_.has_value() to current_tiles_->empty().
  base::Optional<NTPTilesVector> current_tiles_;

  // For callbacks may be run after destruction, used exclusively for TopSites
  // (since it's used to detect whether there's a query in flight).
  base::WeakPtrFactory<MostVisitedSites> top_sites_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
