// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_

#import <Cocoa/Cocoa.h>
#include <map>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#import "chrome/browser/ui/cocoa/main_menu_item.h"
#import "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Profile;
@class HistoryMenuCocoaController;

namespace {
class HistoryMenuBridgeTest;
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
                          public history::HistoryServiceObserver {
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
    base::string16 title;
    // The URL that will be navigated to if the user selects this item.
    GURL url;
    // Favicon for the URL.
    base::scoped_nsobject<NSImage> icon;

    // If the icon is being requested from the FaviconService, |icon_requested|
    // will be true and |icon_task_id| will be valid. If this is false, then
    // |icon_task_id| will be
    // base::CancelableTaskTracker::kBadTaskId.
    bool icon_requested;
    // The Handle given to us by the FaviconService for the icon fetch request.
    base::CancelableTaskTracker::TaskId icon_task_id;

    // The pointer to the item after it has been created. Strong; NSMenu also
    // retains this. During a rebuild flood (if the user closes a lot of tabs
    // quickly), the NSMenu can release the item before the HistoryItem has
    // been fully deleted. If this were a weak pointer, it would result in a
    // zombie.
    base::scoped_nsobject<NSMenuItem> menu_item;

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
    kRecentlyClosedTitle = 401,  // Title of recently closed section.
    kRecentlyClosed = 420,  // Used for items in the recently closed section.
    kVisitedSeparator = 440,  // Separator before visited section.
    kVisitedTitle = 441,  // Title of the visited section.
    kVisited = 460,  // Used for all entries in the visited section.
    kShowFullSeparator = 480  // Separator after the visited section.
  };

  explicit HistoryMenuBridge(Profile* profile);
  ~HistoryMenuBridge() override;

  // TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // MainMenuItem:
  void ResetMenu() override;
  void BuildMenu() override;

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
  NSMenuItem* AddItemToMenu(HistoryItem* item,
                            NSMenu* menu,
                            NSInteger tag,
                            NSInteger index);

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

  // Creates a HistoryItem* for the given tab entry. Caller takes ownership of
  // the result and must delete it when finished.
  HistoryItem* HistoryItemForTab(const sessions::TabRestoreService::Tab& entry);

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
  friend class HistoryMenuCocoaControllerTest;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(history::HistoryService* service) override;

  base::scoped_nsobject<HistoryMenuCocoaController> controller_;  // strong

  Profile* profile_;  // weak
  history::HistoryService* history_service_;  // weak
  sessions::TabRestoreService* tab_restore_service_;  // weak

  base::CancelableTaskTracker cancelable_task_tracker_;

  // Mapping of NSMenuItems to HistoryItems. This owns the HistoryItems until
  // they are removed and deleted via ClearMenuSection().
  std::map<NSMenuItem*, HistoryItem*> menu_item_map_;

  // Requests to re-create the menu are coalesced. |create_in_progress_| is true
  // when either waiting for the history service to return query results, or
  // when the menu is rebuilding. |need_recreate_| is true whenever a rebuild
  // has been scheduled but is waiting for the current one to finish.
  bool create_in_progress_;
  bool need_recreate_;

  // In order to not jarringly refresh the menu while the user has it open,
  // updates are blocked while the menu is tracking.
  bool is_menu_open_;

  // The default favicon if a HistoryItem does not have one.
  base::scoped_nsobject<NSImage> default_favicon_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(HistoryMenuBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_HISTORY_MENU_BRIDGE_H_
