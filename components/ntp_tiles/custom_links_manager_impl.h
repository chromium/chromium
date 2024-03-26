// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_IMPL_H_
#define COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_IMPL_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/custom_links_store.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ntp_tiles {

// Non-test implementation of the CustomLinksManager interface.
class CustomLinksManagerImpl : public CustomLinksManager,
                               public history::HistoryServiceObserver {
 public:
  // Restores the previous state of |current_links_| from prefs.
  CustomLinksManagerImpl(PrefService* prefs,
                         // Can be nullptr in unittests.
                         history::HistoryService* history_service);

  CustomLinksManagerImpl(const CustomLinksManagerImpl&) = delete;
  CustomLinksManagerImpl& operator=(const CustomLinksManagerImpl&) = delete;

  ~CustomLinksManagerImpl() override;

  // CustomLinksManager implementation.
  bool Initialize(const NTPTilesVector& tiles) override;
  void Uninitialize() override;
  bool IsInitialized() const override;

  const std::vector<Link>& GetLinks() const override;

  bool AddLink(const GURL& url, const std::u16string& title) override;
  bool UpdateLink(const GURL& url,
                  const GURL& new_url,
                  const std::u16string& new_title) override;
  bool ReorderLink(const GURL& url, size_t new_pos) override;
  bool DeleteLink(const GURL& url) override;
  bool UndoAction() override;

  base::CallbackListSubscription RegisterCallbackForOnChanged(
      base::RepeatingClosure callback) override;

  // Register preferences used by this class.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:
  void ClearLinks();

  // Stores the current list to the profile's preferences. Does not notify
  // |OnPreferenceChanged|.
  void StoreLinks();

  // Checks during instantiation to remove custom shortcut links
  // created through preinstalled apps.
  void RemoveCustomLinksForPreinstalledApps();

  // Returns an iterator into |custom_links_|.
  std::vector<Link>::iterator FindLinkWithUrl(const GURL& url);

  // history::HistoryServiceObserver implementation.
  // Deletes any Most Visited links whose URL is in |deletion_info|. Clears
  // |previous_links_|. Does not delete entries expired by HistoryService.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Called when the current list of links and/or initialization state in
  // PrefService is modified. Saves the new set of links in |current_links_|
  // and notifies |closure_list_|.
  void OnPreferenceChanged();

  const raw_ptr<PrefService> prefs_;
  CustomLinksStore store_;
  std::vector<Link> current_links_;
  // The state of the current list of links before the last action was
  // performed.
  std::optional<std::vector<Link>> previous_links_;

  // List of closures to be invoked when custom links are updated by outside
  // sources.
  base::RepeatingClosureList closure_list_;

  // Observer for the HistoryService.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Observer for Chrome sync changes to |prefs::kCustomLinksList| and
  // |prefs::kCustomLinksInitialized|.
  PrefChangeRegistrar pref_change_registrar_;
  // Used to ignore notifications from |pref_change_registrar_| that we trigger
  // ourselves when updating the preferences.
  bool updating_preferences_ = false;

  base::WeakPtrFactory<CustomLinksManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_IMPL_H_
