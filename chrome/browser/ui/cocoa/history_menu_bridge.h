// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#import "chrome/browser/ui/cocoa/main_menu_item.h"
#import "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Profile;
class ScopedProfileKeepAlive;
@class HistoryMenuCocoaController;

namespace {
class HistoryMenuBridgeTest;
class HistoryMenuBridgeLifetimeTest;
}

namespace favicon_base {
struct FaviconImageResult;
}

// C++ bridge for the history menu; one per AppController (means there
// is only one). This class observes various data sources, namely the
// HistoryService and the TabRestoreService, and then updates the NSMenu when
// there is new data.
//
// The history menu is broken up into sections: recently visited and recently
// closed. The overall menu has a tag of IDC_HISTORY_MENU, with the user content
// items having the local tags defined in the enum below. Items within a section
// all share the same tag. The structure of the menu is laid out in MainMenu.xib
// and the generated content is inserted after the Title elements. The recently
// closed section is special in that those menu items can have submenus to list
// all the tabs within that closed window. By convention, these submenu items
// have a tag that's equal to the parent + 1. Tags within the history menu have
// a range of [400,500) and do not go through CommandDispatch for their target-
// action mechanism.
//
// These menu items do not use firstResponder as their target. Rather, they are
// hooked directly up to the HistoryMenuCocoaController that then bridges back
// to this class. These items are created via the AddItemToMenu() helper. Also,
// unlike the typical ownership model, this bridge owns its controller. The
// controller is very thin and only exists to interact with Cocoa, but this
// class does the bulk of the work.
class HistoryMenuBridge : public sessions::TabRestoreServiceObserver,
                          public MainMenuItem,
                          public history::HistoryServiceObserver,
                          public ProfileManagerObserver {
 public:
  // This is a generalization of the data we store in the history menu because
  // we pull things from different sources with different data types.
  struct HistoryItem {
   public:
    HistoryItem();
    // Copy constructor allowed.
    HistoryItem(const HistoryItem& copy);
    ~HistoryItem();

    // The title for the menu item.
    std::u16string title;
    // The URL that will be navigated to if the user selects this item.
    GURL url;
    // Favicon for the URL.
    NSImage* __strong icon;

    // If the icon is being requested from the FaviconService, |icon_requested|
    // will be true and |icon_task_id| will be valid. If this is false, then
    // |icon_task_id| will be
    // base::CancelableTaskTracker::kBadTaskId.
    bool icon_requested = false;
    // The Handle given to us by the FaviconService for the icon fetch request.
    base::CancelableTaskTracker::TaskId icon_task_id =
        base::CancelableTaskTracker::kBadTaskId;

    // The pointer to the item after it has been created. Strong; NSMenu also
    // retains this. During a rebuild flood (if the user closes a lot of tabs
    // quickly), the NSMenu can release the item before the HistoryItem has
    // been fully deleted. If this were a weak pointer, it would result in a
    // zombie.
    NSMenuItem* __strong menu_item;

    // This ID is unique for a browser session and can be passed to the
    // TabRestoreService to re-open the closed window or tab that this
    // references. A valid session ID indicates that this is an entry can be
    // restored that way. Otherwise, the URL will be used to open the item and
    // this ID will be SessionID::InvalidValue().
    SessionID session_id;

    // If the HistoryItem is a window, this will be the vector of tabs. Note
    // that this is a list of weak references. The |menu_item_map_| is the owner
    // of all items. If it is not a window, then the entry is a single page and
    // the vector will be empty.
    std::vector<HistoryItem*> tabs;

   private:
    // Copying is explicitly allowed, but assignment is not.
    void operator=(const HistoryItem&);
  };

  // These tags are not global view tags and are local to the history menu. The
  // normal procedure for menu items is to go through CommandDispatch, but since
  // history menu items are hooked directly up to their target, they do not need
  // to have the global IDC view tags.
  enum Tags {
    kRecentlyClosedSeparator = 400,  // Item before recently closed section.
    kRecentlyClosedTitle = 401,      // Title of recently closed section.
    kRecentlyClosed = 420,    // Used for items in the recently closed section.
    kVisitedSeparator = 440,  // Separator before visited section.
    kVisitedTitle = 441,      // Title of the visited section.
    kVisited = 460,           // Used for all entries in the visited section.
    kShowFullSeparator = 480  // Separator after the visited section.
  };

  explicit HistoryMenuBridge(Profile* profile);

  HistoryMenuBridge(const HistoryMenuBridge&) = delete;
  HistoryMenuBridge& operator=(const HistoryMenuBridge&) = delete;

  ~HistoryMenuBridge() override;

  // TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // MainMenuItem:
  void ResetMenu() override;
  void BuildMenu() override;

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Looks up an NSMenuItem in the |menu_item_map_| and returns the
  // corresponding HistoryItem.
  HistoryItem* HistoryItemForMenuItem(NSMenuItem* item);

  // Called by HistoryMenuCocoaController when the menu begins and ends
  // tracking, to block updates when it is open.
  void SetIsMenuOpen(bool flag);

  // I wish I has a "friend @class" construct. These are used by the HMCC
  // to access model information when responding to actions.
  history::HistoryService* service();
  Profile* profile();
  const base::FilePath& profile_dir() const;

  // Resets |profile_| to nullptr. Called before the Profile is destroyed, when
  // this bridge is still needed. Also performs some internal cleanup, like
  // resetting observers and pointers to the Profile and KeyedServices.
  void OnProfileWillBeDestroyed();

 protected:
  // Return the History menu.
  virtual NSMenu* HistoryMenu();

  // Clear items in the given |menu|. Menu items in the same section are given
  // the same tag. This will go through the entire history menu, removing all
  // items with a given tag. Note that this will recurse to submenus, removing
  // child items from the menu item map. This will only remove items that have
  // a target hooked up to the |controller_|.
  void ClearMenuSection(NSMenu* menu, NSInteger tag);

  // Adds a given title and URL to the passed-in menu with a certain tag and
  // index. This will add |item| and the newly created menu item to the
  // |menu_item_map_|, which takes ownership. Items are deleted in
  // ClearMenuSection(). This returns the new menu item that was just added.
  NSMenuItem* AddItemToMenu(std::unique_ptr<HistoryItem> item,
                            NSMenu* menu,
                            NSInteger tag,
                            NSInteger index);

  // Adds an item for the window entry with a submenu containing its tabs.
  // Returns whether the item was successfully added.
  bool AddWindowEntryToMenu(sessions::tab_restore::Window* window,
                            NSMenu* menu,
                            NSInteger tag,
                            NSInteger index);

  // Adds an item for the group entry with a submenu containing its tabs.
  // Returns whether the item was successfully added.
  bool AddGroupEntryToMenu(sessions::tab_restore::Group* group,
                           NSMenu* menu,
                           NSInteger tag,
                           NSInteger index);

  // Adds standard 'Restore All' items and an item for each tab in |tabs|,
  // potentially filtering out tabs like the NTP. Returns the number of tabs
  // successfully added and updates the HistoryItem with those tabs.
  int AddTabsToSubmenu(
      NSMenu* submenu,
      HistoryItem* item,
      const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs);

  // Called by the ctor if |service_| is ready at the time, or by a
  // notification receiver. Finishes initialization tasks by subscribing for
  // change notifications and calling CreateMenu().
  void Init();

  // Does the query for the history information to create the menu.
  void CreateMenu();

  // Invoked when the History information has changed.
  void OnHistoryChanged();

  // Callback method for when HistoryService query results are ready with the
  // most recently-visited sites.
  void OnVisitedHistoryResults(history::QueryResults results);

  // Creates a HistoryItem* for the given tab entry.
  std::unique_ptr<HistoryItem> HistoryItemForTab(
      const sessions::tab_restore::Tab& entry);

  // Helper function that sends an async request to the FaviconService to get
  // an icon. The callback will update the NSMenuItem directly.
  void GetFaviconForHistoryItem(HistoryItem* item);

  // Callback for the FaviconService to return favicon image data when we
  // request it. This decodes the raw data, updates the HistoryItem, and then
  // sets the image on the menu. Called on the same same thread that
  // GetFaviconForHistoryItem() was called on (UI thread).
  void GotFaviconData(HistoryItem* item,
                      const favicon_base::FaviconImageResult& image_result);

  // Cancels a favicon load request for a given HistoryItem, if one is in
  // progress.
  void CancelFaviconRequest(HistoryItem* item);

 private:
  friend class ::HistoryMenuBridgeTest;
  friend class ::HistoryMenuBridgeLifetimeTest;
  friend class HistoryMenuCocoaControllerTest;

  void FinishCreateMenu();

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(history::HistoryService* service) override;
  void HistoryServiceBeingDeleted(history::HistoryService* service) override;

  // Changes the visibility of the menu items depend on the current profile
  // type.
  void SetVisibilityOfMenuItems();

  // Returns if the given menu item should be visible for the current profile.
  bool ShouldMenuItemBeVisible(NSMenuItem* item);

  HistoryMenuCocoaController* __strong controller_;

  raw_ptr<Profile> profile_;                                            // weak
  raw_ptr<history::HistoryService> history_service_ = nullptr;          // weak
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;  // weak
  base::FilePath profile_dir_;  // Remembered after OnProfileWillBeDestroyed().

  // Inhibit Profile destruction until the HistoryService and TabRestoreService
  // are finished loading.
  std::unique_ptr<ScopedProfileKeepAlive> history_service_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> tab_restore_service_keep_alive_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  // A timer used to coalesce repeated calls to CreateMenu().
  base::OneShotTimer finish_create_menu_timer_;

  // Mapping of NSMenuItems to HistoryItems.
  std::map<NSMenuItem*, std::unique_ptr<HistoryItem>> menu_item_map_;

  // Requests to re-create the menu are coalesced. |create_in_progress_| is true
  // when either waiting for the history service to return query results, or
  // when the menu is rebuilding. |need_recreate_| is true whenever a rebuild
  // has been scheduled but is waiting for the current one to finish.
  bool create_in_progress_ = false;
  bool need_recreate_ = false;

  // In order to not jarringly refresh the menu while the user has it open,
  // updates are blocked while the menu is tracking.
  bool is_menu_open_ = false;

  // The default favicon if a HistoryItem does not have one.
  NSImage* __strong default_favicon_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      tab_restore_service_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<HistoryMenuBridge> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
