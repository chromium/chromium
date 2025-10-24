// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
#define COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/ntp_tiles/section_type.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/webapps/common/constants.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#endif

namespace signin {
class IdentityManager;
}

namespace supervised_user {
class SupervisedUserService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace ntp_tiles {

class IconCacher;

// NTPTilesVector wrapper with HasUrl(), to store Custom Links.
class CustomLinksCache {
 public:
  CustomLinksCache();
  ~CustomLinksCache();

  // Adds a tile to the list.
  void PushBack(const NTPTile& tile);

  // Removes all stored tiles.
  void Clear();

  // Returns whether a tile with specified `url` exists.
  bool HasUrl(const GURL& url) const;

  // Accessor to stored tiles.
  const NTPTilesVector& GetList() const;

 private:
  // List of custom tiles, in the order of appearance, with distinct URLs.
  NTPTilesVector list_;

  // Set of URLs in |list|, for deduping.
  std::set<GURL> url_set_;
};

// Tracks the list of most visited sites.
class MostVisitedSites :
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    public SupervisedUserServiceObserver,
#endif
    public history::TopSitesObserver {
 public:
  // LINT.IfChange(kInvalidSuggestionScore)
  // Value to indicate that a site suggestion score is unavailable.
  static constexpr double kInvalidSuggestionScore = -1.0;
  // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/suggestions/mostvisited/MostVisitedSites.java)

  // The observer to be notified when the list of most visited sites changes.
  class Observer : public base::CheckedObserver {
   public:
    // |is_user_triggered| specifies whether the event is caused by direct user
    // action in MV tiles. The UI can use this to decide whether MV tile updates
    // (in multiple NTPs) should be eager (for responsiveness) or deferred (for
    // tile stability).
    // |sections| must at least contain the PERSONALIZED section.
    virtual void OnURLsAvailable(
        bool is_user_triggered,
        const std::map<SectionType, NTPTilesVector>& sections) = 0;
    virtual void OnIconMadeAvailable(const GURL& site_url) = 0;
  };

  // This interface delegates the retrieval of the homepage to the
  // platform-specific implementation.
  class HomepageClient {
   public:
    using TitleCallback =
        base::OnceCallback<void(const std::optional<std::u16string>& title)>;

    virtual ~HomepageClient() = default;
    virtual bool IsHomepageTileEnabled() const = 0;
    virtual GURL GetHomepageUrl() const = 0;
    virtual void QueryHomepageTitle(TitleCallback title_callback) = 0;
  };

  // Construct a MostVisitedSites instance.
  //
  // |prefs| are required and may not be null. |top_sites|,
  // |popular_sites|, |custom_links|, |enterprise_shortcuts|,
  // |identity_manager|, |supervised_user_service| and |homepage_client| are
  //  optional and if null, the associated features will be disabled.
  MostVisitedSites(
      PrefService* prefs,
      signin::IdentityManager* identity_manager,
      supervised_user::SupervisedUserService* supervised_user_service,
      scoped_refptr<history::TopSites> top_sites,
      std::unique_ptr<PopularSites> popular_sites,
      std::unique_ptr<CustomLinksManager> custom_links,
      std::unique_ptr<EnterpriseShortcutsManager> enterprise_shortcuts,
      std::unique_ptr<IconCacher> icon_cacher,
      bool is_default_chrome_app_migrated);

  MostVisitedSites(const MostVisitedSites&) = delete;
  MostVisitedSites& operator=(const MostVisitedSites&) = delete;

  ~MostVisitedSites() override;

  // Returns true if this object was created with a non-null provider for the
  // given NTP tile source. That source may or may not actually provide tiles,
  // depending on its configuration and the priority of different sources.
  bool DoesSourceExist(TileSource source) const;

  // Returns the corresponding object passed at construction.
  history::TopSites* top_sites() { return top_sites_.get(); }
  PopularSites* popular_sites() { return popular_sites_.get(); }

  // Adds the observer and immediately fetches the current suggestions.
  // All observers will be notified when the suggestions are fetched.
  //
  // Note: only observers that require the same |max_num_sites| could observe
  // the same MostVisitedSites instance. Otherwise, a new Instance should be
  // created for the observer.
  //
  // Does not take ownership of |observer|, which must outlive this object and
  // must not be null. |max_num_sites| indicates the the maximum number of most
  // visited sites to return.
  virtual void AddMostVisitedURLsObserver(Observer* observer,
                                          size_t max_num_sites);

  // Removes the observer.
  virtual void RemoveMostVisitedURLsObserver(Observer* observer);

  // Sets the client that provides platform-specific homepage preferences.
  // When used to replace an existing client, the new client will first be
  // used during the construction of a new tile set.
  // |client| must not be null and outlive this object.
  void SetHomepageClient(std::unique_ptr<HomepageClient> client);

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
  bool IsCustomLinksInitialized() const;

  // TODO(crbug.com/454775651): Look into renaming this to a more accurate
  // description like `AssignTileTypesEnablement()`.
  // Options for MostVisitedSites::EnableTileTypes. By default, all tile types
  // are disabled.
  struct EnableTileTypesOptions {
    EnableTileTypesOptions& with_top_sites(bool b) {
      enable_top_sites = b;
      return *this;
    }

    EnableTileTypesOptions& with_custom_links(bool b) {
      enable_custom_links = b;
      return *this;
    }

    EnableTileTypesOptions& with_enterprise_shortcuts(bool b) {
      enable_enterprise_shortcuts = b;
      return *this;
    }

    bool operator==(const EnableTileTypesOptions&) const = default;

    bool enable_top_sites = false;
    bool enable_custom_links = false;
    bool enable_enterprise_shortcuts = false;
  };

  // Sets the type of shortcuts to show, but does not (un)initialize them.
  // Called when the user switches between custom links and Most Visited sites
  // on the 1P Desktop NTP.
  void EnableTileTypes(const EnableTileTypesOptions& options);

  // Returns whether top sites are enabled.
  bool IsTopSitesEnabled() const;

  // Returns whether custom links are enabled.
  bool IsCustomLinksEnabled() const;

  // Returns whether managed shortcuts are enabled.
  bool IsEnterpriseShortcutsEnabled() const;

  // Sets the visibility of the NTP tiles.
  void SetShortcutsVisible(bool visible);

  // Returns whether NTP tiles should be shown.
  bool IsShortcutsVisible() const;

  // Adds a custom link at position |pos|, bumping existing links. If the number
  // of current links is maxed, returns false and does nothing. Will initialize
  // custom links if they have not been initialized yet, unless the action
  // fails. Custom links must be enabled.
  bool AddCustomLinkTo(const GURL& url,
                       const std::u16string& title,
                       size_t pos);

  // Similar to AddCustomLinkTo(), but add to end of list.
  bool AddCustomLink(const GURL& url, const std::u16string& title);

  // Updates the URL and/or title of the custom link specified by |url|. If
  // |url| does not exist or |new_url| already exists in the custom link list,
  // returns false and does nothing. Will initialize custom links if they have
  // not been initialized yet, unless the action fails. Custom links must be
  // enabled.
  bool UpdateCustomLink(const GURL& url,
                        const GURL& new_url,
                        const std::u16string& new_title);

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

  // Returns whether a custom link with the specified |url| exists.
  bool HasCustomLink(const GURL& url);

  // Restores the previous state of custom links before the last action that
  // modified them. If there was no action, does nothing. If this is undoing the
  // first action after initialization, uninitializes the links. Custom links
  // must be enabled.
  void UndoCustomLinkAction();

  size_t GetCustomLinkNum();

  // Restores the enterprise shortcuts to the state defined by policy.
  void RestoreEnterpriseShortcutsDefaults();

  // Updates the title of the enterprise shortcut specified by |url|. Returns
  // false and does nothing if enterprise shortcuts are not enabled or |url|
  // does not exist.
  bool UpdateEnterpriseShortcut(const GURL& url, const std::u16string& title);

  // Moves the enterprise shortcut specified by |url| to the index |new_pos|.
  // Returns false and does nothing if enterprise shortcuts are not enabled,
  // |url| does not exist, or |new_pos| is invalid.
  bool ReorderEnterpriseShortcut(const GURL& url, size_t new_pos);

  // Hides the enterprise shortcut with the specified |url|. Returns false and
  // does nothing if enterprise shortcuts are not enabled or |url| does not
  // exist.
  bool DeleteEnterpriseShortcut(const GURL& url);

  // Restores the previous state of enterprise shortcuts before the last action
  // that modified them. Returns false and does nothing if enterprise shortcuts
  // are not enabled or there is no previous state to restore.
  bool UndoEnterpriseShortcutAction();

  void AddOrRemoveBlockedUrl(const GURL& url, bool add_url);
  void ClearBlockedUrls();

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  //  SupervisedUserServiceObserver implementation.
  void OnURLFilterChanged() override;
#endif

  // Returns the score of a tile in |current_tiles_| identified by |url|, or
  // |kInvalidSuggestionScore| if not found. Caveat: On startup,
  // |current_tiles_| may store cached values, so returned score will be 0.0.
  // In this case, the caller needs to be robust against 0.0, or first force a
  // rebuild by calling RefreshTiles().
  double GetSuggestionScore(const GURL& url) const;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void ResetProfilePrefs(PrefService* prefs);

  // Workhorse for SaveNewTilesAndNotify. Implemented as a separate static and
  // public method for ease of testing.
  static NTPTilesVector MergeTiles(NTPTilesVector personal_tiles,
                                   NTPTilesVector popular_tiles);

  // Verifies if NTPTile App was migrated to a WebApp.
  static bool WasNtpAppMigratedToWebApp(PrefService* prefs, GURL url);

  // Verifies if NTPTile App comes from a PreInstalledApp.
  static bool IsNtpTileFromPreinstalledApp(GURL url);

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

  // Returns the maximum number of most visited sites to return. The return
  // value is |max_num_sites_| which is ntp_tiles::kMaxNumMostVisited for
  // Desktop, unless custom links are enabled in which case an additional tile
  // may be returned making up to ntp_tiles::kMaxNumCustomLinks custom links
  // including the "Add shortcut" button.
  size_t GetMaxNumSites() const;

  // Initialize the query to Top Sites.
  void InitiateTopSitesQuery(bool is_user_triggered);

  // Returns enterprise shortcut tiles.
  NTPTilesVector GetEnterpriseShortcutTiles();

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      bool is_user_triggered,
      const history::MostVisitedURLList& visited_list);

  // Builds the current tileset based on available caches and notifies the
  // observer.
  void BuildCurrentTiles(bool is_user_triggered);

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

  // Ensures |custom_links_manager_| is initialized, then runs
  // |custom_links_action|. Performs on-failure cleanup. Returns whether the
  // action was successful.
  bool ApplyCustomLinksAction(base::OnceCallback<bool()> custom_links_action);

  // Ensures |enterprise_shortcuts_manager_| exists and
  // |is_enterprise_shortcuts_enabled_| is true, then runs
  // |enterprise_shortcuts_action|. Does nothing if action fails. Returns
  // whether the action was successful.
  bool ApplyEnterpriseShortcutsAction(
      base::OnceCallback<bool()> enterprise_shortcuts_action);

  // Callback for when an update is reported by CustomLinksManager.
  void OnCustomLinksChanged();

  // Callback for when an update is reported by EnterpriseShortcutsManager.
  void OnEnterpriseShortcutsChanged();

  // Clears |custom_links_cache_|, then if custom links are initialized,
  // populate it with |custom_links_manager_->GetLinks()| data up to
  // |max_num_sites_|.
  void ReloadCustomLinksCache();

  // Initiates a query for the homepage tile if needed and calls
  // |SaveTilesAndNotify| in the end.
  void InitiateNotificationForNewTiles(bool is_user_triggered,
                                       NTPTilesVector new_tiles);

  // Takes the personal tiles and merges in popular tiles if appropriate. Calls
  // |SaveTilesAndNotify| at the end.
  void MergeMostVisitedTiles(bool is_user_triggered,
                             NTPTilesVector personal_tiles);

  // Removes pre installed apps which turn invalid because of migration.
  NTPTilesVector RemoveInvalidPreinstallApps(NTPTilesVector new_tiles);

  // Creates a new tiles vector consisting of |custom_links_cache_| combined
  // with |tiles|.
  NTPTilesVector ImposeCustomLinks(NTPTilesVector tiles);

  // Creates a new tiles vector consisting of GetEnterpriseShortcutTiles()
  // combined with |tiles|.
  NTPTilesVector ImposeEnterpriseShortcuts(NTPTilesVector tiles);

  // Saves the new tiles and notifies the observer if the tiles were actually
  // changed.
  void SaveTilesAndNotify(bool is_user_triggered,
                          NTPTilesVector new_tiles,
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
                                const std::u16string& title) const;

  void OnHomepageTitleDetermined(bool is_user_triggered,
                                 NTPTilesVector tiles,
                                 const std::optional<std::u16string>& title);

  // Returns true if there is a valid homepage that can be pinned as tile.
  bool ShouldAddHomeTile() const;

  // Returns true if top sites should be queried.
  bool ShouldQueryTopSites() const;

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  raw_ptr<PrefService> prefs_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<supervised_user::SupervisedUserService> supervised_user_service_;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  base::ScopedObservation<supervised_user::SupervisedUserService,
                          SupervisedUserServiceObserver>
      supervised_user_service_observation_{this};
#endif

  scoped_refptr<history::TopSites> top_sites_;
  std::unique_ptr<PopularSites> const popular_sites_;
  std::unique_ptr<CustomLinksManager> const custom_links_manager_;
  std::unique_ptr<EnterpriseShortcutsManager> const
      enterprise_shortcuts_manager_;
  std::unique_ptr<IconCacher> const icon_cacher_;
  std::unique_ptr<HomepageClient> homepage_client_;
  bool is_default_chrome_app_migrated_;

  base::ObserverList<Observer> observers_;

  // The maximum number of most visited sites to return.
  // Do not use directly. Use GetMaxNumSites() instead.
  size_t max_num_sites_;

  // Number of actions after custom link initialization. Set to -1 and not
  // incremented if custom links was not initialized during this session.
  int custom_links_action_count_ = -1;

  EnableTileTypesOptions enabled_tile_types_ =
      EnableTileTypesOptions().with_custom_links(true);
  bool is_shortcuts_visible_ = true;

  base::ScopedObservation<history::TopSites, history::TopSitesObserver>
      top_sites_observation_{this};

  base::CallbackListSubscription custom_links_subscription_;
  base::CallbackListSubscription enterprise_shortcuts_subscription_;

  // Cached custom links data that also supports URL existence query.
  CustomLinksCache custom_links_cache_;

  // Current set of tiles. Optional so that the observer can be notified
  // whenever it changes, including possibily an initial change from
  // !current_tiles_.has_value() to current_tiles_->empty().
  std::optional<NTPTilesVector> current_tiles_;

  // Whether has started observing data sources.
  bool is_observing_;

  // For callbacks may be run after destruction, used exclusively for TopSites
  // (since it's used to detect whether there's a query in flight).
  base::WeakPtrFactory<MostVisitedSites> top_sites_weak_ptr_factory_{this};
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
