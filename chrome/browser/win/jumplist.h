// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_H_
#define CHROME_BROWSER_WIN_JUMPLIST_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace chrome {
struct FaviconImageResult;
}

class JumpListFactory;
class PrefChangeRegistrar;
class Profile;

// A class which implements an application JumpList.
//
// This class observes the recently closed tabs, topsites, and policies of the
// given Profile. It tries to update the JumpList whenever a change is detected.
//
// The states that the JumpList machine can be in between UI thread tasks are:
// 1. Idle.
// 2. Notification from TabRetore and/or TopSites services arrived, waiting for
//    quiesence via a timer.
// 3. Most-visited data async fetch started, waiting for data. (Tab restore data
//    is fetched synchronously, so it doesn't have a state.)
// 4. Tab restore data or most-visited data has been fetched; favicons load
//    started (one at a time), waiting for results.
// 5. All favicon loaded, JumpList update started.
//
// The policy for how to handle notifications while any state of an update is in
// progress is:
//   "Prohibit overlapping updates by queueing up inbound notifications that
// arrive after an update has started but before it has finished. On update
// completion, restart the timer to start another update if any notifications
// were queued."
//
// Updating a JumpList as mentioned in state 5 requires some file operations and
// it is not good to run it in the UI thread. To solve this problem, this class
// posts the update task to a non-UI thread.
//
// Updating a JumpList consists of the following steps:
// 1. Tell the OS to begin an update transaction. If this fails or times out,
//    abort this update run.
// 2. Create new icon files if not in the cache.
// 3. Assemble an updated JumpList with most-visited, recently-closed and tasks
//    (never change) categories. If this fails or times out, go to step 5.
// 4. Commit the update to the OS.
// 5. Delete any obsolete icon files. If step 3 or 4 fails or times out, delete
//    icons newly created in step 2, otherwise delete icons from the previous
//    JumpList but aren't reused by the new one.
// 6. Update the icon cache.
//
// Step 1, 3 and 4 are basically Windows API calls. Normally they are pretty
// fast (within a few hundred milliseconds), but can take minutes in slow
// machines. To prevent those machines from being bogged down by JumpList
// updates, this class skips the next few updates if a timeout is detected in
// any of those steps.
class JumpList : public sessions::TabRestoreServiceObserver,
                 public history::TopSitesObserver,
                 public KeyedService {
 public:
  // Returns true if the custom JumpList is enabled.
  static bool Enabled();

  // KeyedService:
  void Shutdown() override;

 private:
  using UrlAndLinkItem = std::pair<std::string, scoped_refptr<ShellLinkItem>>;
  using URLIconCache = base::flat_map<std::string, base::FilePath>;

  // Holds results of a RunUpdateJumpList run.
  // In-out params:
  //   |most_visited_icons|, |recently_closed_icons|
  // Out params:
  //   |update_success|, |update_timeout|
  struct UpdateTransaction {
    UpdateTransaction();
    ~UpdateTransaction();

    // Icon file paths of the most visited links, indexed by tab url.
    // Holding a copy of most_visited_icons_ initially, it's updated by the
    // JumpList update run. If the update run succeeds, it overwrites
    // most_visited_icons_.
    URLIconCache most_visited_icons;

    // Icon file paths of the recently closed links, indexed by tab url.
    // Holding a copy of recently_closed_icons_ initially, it's updated by the
    // JumpList update run. If the update run succeeds, it overwrites
    // recently_closed_icons_.
    URLIconCache recently_closed_icons;

    // A flag indicating if a JumpList update run is successful.
    bool update_success = false;

    // A flag indicating if there is a timeout in notifying the JumpList update
    // to shell. Note that this variable is independent of update_success.
    bool update_timeout = false;
  };

  friend JumpListFactory;
  explicit JumpList(Profile* profile);  // Use JumpListFactory instead

  ~JumpList() override;

  // history::TopSitesObserver:
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // Callback for changes to the incognito mode availability pref.
  void OnIncognitoAvailabilityChanged();

  // Initializes the one-shot timer to update the JumpList in a while. If there
  // is already a request queued then cancel it and post the new request. This
  // ensures that JumpList update won't happen until there has been a brief
  // quiet period, thus avoiding update storms.
  void InitializeTimerForUpdate();

  // Processes update notifications. Calls APIs ProcessTopSitesNotification and
  // ProcessTabRestoreNotification on demand to do the actual work.
  void ProcessNotifications();

  // Processes notifications from TopSites service.
  void ProcessTopSitesNotification();

  // Processes notifications from TabRestore service.
  void ProcessTabRestoreServiceNotification();

  // Callback for TopSites that notifies when |data|, the "Most Visited" list,
  // is available. This function updates the ShellLinkItemList objects and
  // begins the process of fetching favicons for the URLs.
  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data);

  // Adds a new ShellLinkItem for |tab| to the JumpList data provided that doing
  // so will not exceed |max_items|.
  bool AddTab(const sessions::TabRestoreService::Tab& tab, size_t max_items);

  // Adds a new ShellLinkItem for each tab in |window| to the JumpList data
  // provided that doing so will not exceed |max_items|.
  void AddWindow(const sessions::TabRestoreService::Window& window,
                 size_t max_items);

  // Starts loading a favicon for each URL in |icon_urls_|.
  // This function sends a query to HistoryService.
  // When finishing loading all favicons, this function posts a task that
  // decompresses collected favicons and updates a JumpList.
  void StartLoadingFavicon();

  // Callback for HistoryService that notifies when a requested favicon is
  // available. To avoid file operations, this function just attaches the given
  // |image_result| to a ShellLinkItem object.
  void OnFaviconDataAvailable(
      const favicon_base::FaviconImageResult& image_result);

  // Posts tasks to update the JumpList and delete any obsolete JumpList related
  // folders.
  void PostRunUpdate();

  // Handles the completion of an update by incorporating its results in
  // |update_transaction| back into this instance. Additionally, a new update is
  // triggered as needed to process notifications that arrived while the
  // now-completed update was running.
  void OnRunUpdateCompletion(
      std::unique_ptr<UpdateTransaction> update_transaction);

  // Cancels a pending JumpList update.
  void CancelPendingUpdate();

  // Terminates the JumpList, which includes cancelling any pending updates and
  // stopping observing the Profile and its services. This must be called before
  // the |profile_| is destroyed.
  void Terminate();

  // Updates the application JumpList, which consists of 1) create a new
  // JumpList along with any icons that are not in the cache; 2) notify the OS;
  // 3) delete obsolete icon files. Any error along the way results in the old
  // JumpList being left as-is.
  static void RunUpdateJumpList(
      const base::string16& app_id,
      const base::FilePath& profile_dir,
      const ShellLinkItemList& most_visited_pages,
      const ShellLinkItemList& recently_closed_pages,
      bool most_visited_should_update,
      bool recently_closed_should_update,
      IncognitoModePrefs::Availability incognito_availability,
      UpdateTransaction* update_transaction);

  // Creates a new JumpList along with any icons that are not in the cache,
  // and notifies the OS.
  static void CreateNewJumpListAndNotifyOS(
      const base::string16& app_id,
      const base::FilePath& most_visited_icon_dir,
      const base::FilePath& recently_closed_icon_dir,
      const ShellLinkItemList& most_visited_pages,
      const ShellLinkItemList& recently_closed_pages,
      bool most_visited_should_update,
      bool recently_closed_should_update,
      IncognitoModePrefs::Availability incognito_availability,
      UpdateTransaction* update_transaction);

  // Updates icon files for |item_list| in |icon_dir|, which consists of
  // 1) If certain safe conditions are not met, clean the folder at |icon_dir|.
  // If folder cleaning fails, skip step 2. Besides, clear |icon_cur| and
  // |icon_next|.
  // 2) Create at most |max_items| icon files which are not in |icon_cur| for
  // the asynchrounously loaded icons stored in |item_list|.
  static int UpdateIconFiles(const base::FilePath& icon_dir,
                             const ShellLinkItemList& item_list,
                             size_t max_items,
                             URLIconCache* icon_cur,
                             URLIconCache* icon_next);

  // In |icon_dir|, creates at most |max_items| icon files which are not in
  // |icon_cur| for the asynchrounously loaded icons stored in |item_list|.
  // |icon_next| is updated based on the reusable icons and the newly created
  // icons. Returns the number of new icon files created.
  static int CreateIconFiles(const base::FilePath& icon_dir,
                             const ShellLinkItemList& item_list,
                             size_t max_items,
                             const URLIconCache& icon_cur,
                             URLIconCache* icon_next);

  // Deletes icon files in |icon_dir| which are not in |icon_cache|.
  static void DeleteIconFiles(const base::FilePath& icon_dir,
                              const URLIconCache& icons_cache);

  // Tracks FaviconService tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The Profile object is used to listen for events.
  Profile* profile_;

  // Manages the registration of pref change observers.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // App id to associate with the JumpList.
  base::string16 app_id_;

  // Timer for requesting delayed JumpList updates.
  base::OneShotTimer timer_;

  // A list of URLs we need to retrieve their favicons,
  std::list<UrlAndLinkItem> icon_urls_;

  // Items in the "Most Visited" category of the JumpList.
  ShellLinkItemList most_visited_pages_;

  // Items in the "Recently Closed" category of the JumpList.
  ShellLinkItemList recently_closed_pages_;

  // The icon file paths of the most visited links, indexed by tab url.
  URLIconCache most_visited_icons_;

  // The icon file paths of the recently closed links, indexed by tab url.
  URLIconCache recently_closed_icons_;

  // A flag indicating if TopSites service has notifications.
  bool top_sites_has_pending_notification_ = false;

  // A flag indicating if TabRestore service has notifications.
  bool tab_restore_has_pending_notification_ = false;

  // A flag indicating if "Most Visited" category should be updated.
  bool most_visited_should_update_ = false;

  // A flag indicating if "Recently Closed" category should be updated.
  bool recently_closed_should_update_ = false;

  // A flag indicating if there's a JumpList update task already posted or
  // currently running.
  bool update_in_progress_ = false;

  // A flag indicating if a session has at least one tab closed.
  bool has_tab_closed_ = false;

  // A flag indicating if a session has at least one top sites sync.
  bool has_topsites_sync = false;

  // Number of updates to skip to alleviate the machine when a previous update
  // was too slow. Updates will be resumed when this reaches 0 again.
  int updates_to_skip_ = 0;

  // Id of last favicon task. It's used to cancel current task if a new one
  // comes in before it finishes.
  base::CancelableTaskTracker::TaskId task_id_ =
      base::CancelableTaskTracker::kBadTaskId;

  // A task runner running tasks to update the JumpList.
  scoped_refptr<base::SingleThreadTaskRunner> update_jumplist_task_runner_;

  // A task runner running tasks to delete the JumpListIcons and
  // JumpListIconsOld folders.
  scoped_refptr<base::SequencedTaskRunner> delete_jumplisticons_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // For callbacks may run after destruction.
  base::WeakPtrFactory<JumpList> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JumpList);
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_H_
