// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"

class Browser;
class BrowserWindowInterface;
class GURL;

namespace tabs {
class TabModel;
}

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace tab_groups {
class TabGroupId;
}

///////////////////////////////////////////////////////////////////////////////
//
// TabStripModelDelegate
//
//  A delegate interface that the TabStripModel uses to perform work that it
//  can't do itself, such as obtain a container HWND for creating new
//  WebContentses, creating new TabStripModels for detached tabs, etc.
//
//  This interface is typically implemented by the controller that instantiates
//  the TabStripModel (in our case the Browser object).
//
///////////////////////////////////////////////////////////////////////////////
class TabStripModelDelegate {
 public:
  enum {
    TAB_MOVE_ACTION = 1,
    TAB_TEAROFF_ACTION = 2
  };

  virtual ~TabStripModelDelegate() {}

  // Adds a tab to the model and loads |url| in the tab. If |url| is an empty
  // URL, then the new tab-page is loaded instead. An |index| value of -1
  // means to append the contents to the end of the tab strip.
  virtual void AddTabAt(
      const GURL& url,
      int index,
      bool foreground,
      std::optional<tab_groups::TabGroupId> group = std::nullopt) = 0;

  // Asks for a new TabStripModel to be created and the given web contentses to
  // be added to it. Its size and position are reflected in |window_bounds|.
  // Returns the Browser object representing the newly created window and tab
  // strip. This does not show the window; it's up to the caller to do so.
  //
  // TODO(avi): This is a layering violation; the TabStripModel should not know
  // about the Browser type. At least fix so that this returns a
  // TabStripModelDelegate, or perhaps even move this code elsewhere.
  struct NewStripContents {
    NewStripContents();
    NewStripContents(const NewStripContents&) = delete;
    NewStripContents& operator=(const NewStripContents&) = delete;
    ~NewStripContents();
    NewStripContents(NewStripContents&&);
    // The TabModel to add.
    std::unique_ptr<tabs::TabModel> tab;
    // A bitmask of TabStripModel::AddTabTypes to apply to the added contents.
    int add_types = 0;
  };
  virtual Browser* CreateNewStripWithTabs(std::vector<NewStripContents> tabs,
                                          const gfx::Rect& window_bounds,
                                          bool maximize) = 0;

  // Notifies the delegate that the specified WebContents will be added to the
  // tab strip (via insertion/appending/replacing existing) and allows it to do
  // any preparation that it deems necessary.
  virtual void WillAddWebContents(content::WebContents* contents) = 0;

  // Determines what drag actions are possible for the specified strip.
  virtual int GetDragActions() const = 0;

  // Returns whether some contents can be duplicated.
  virtual bool CanDuplicateContentsAt(int index) = 0;

  // Returns whether tabs can be highlighted. This may return false due to tab
  // dragging in process, for instance.
  virtual bool IsTabStripEditable() = 0;

  // Duplicates the contents at the provided index and places it into a new tab.
  virtual void DuplicateContentsAt(int index) = 0;

  // Move the contents at the provided indices into the specified window.
  virtual void MoveToExistingWindow(const std::vector<int>& indices,
                                    int browser_index) = 0;

  // Returns whether the contents at |indices| can be moved from the current
  // tabstrip to a different window.
  virtual bool CanMoveTabsToWindow(const std::vector<int>& indices) = 0;

  // Removes the contents at |indices| from this tab strip and places it into a
  // new window.
  virtual void MoveTabsToNewWindow(const std::vector<int>& indices) = 0;

  // Moves all the tabs in the specified |group| to a new window, keeping them
  // grouped. The group in the new window will have the same appearance as
  // |group| but a different ID, since IDs can't be shared across windows.
  virtual void MoveGroupToNewWindow(const tab_groups::TabGroupId& group) = 0;

  // Creates an entry in the historical tab database for the specified
  // WebContents. Returns the tab's unique SessionID if a historical tab was
  // created.
  virtual std::optional<SessionID> CreateHistoricalTab(
      content::WebContents* contents) = 0;

  // Creates an entry in the historical group database for the specified
  // |group|.
  virtual void CreateHistoricalGroup(const tab_groups::TabGroupId& group) = 0;

  // Called on group creation after the group has been added to the tabstrip and
  // all tabs have been added.
  virtual void GroupAdded(const tab_groups::TabGroupId& group) = 0;

  // Notifies the delegate that a group is about to be closed, and allows it
  // to perform any preparation neccessary.
  virtual void WillCloseGroup(const tab_groups::TabGroupId& group) = 0;

  // Notifies the tab restore service that the group is no longer closing.
  virtual void GroupCloseStopped(const tab_groups::TabGroupId& group) = 0;

  // Runs any unload listeners associated with the specified WebContents
  // before it is closed. If there are unload listeners that need to be run,
  // this function returns true and the TabStripModel will wait before closing
  // the WebContents. If it returns false, there are no unload listeners
  // and the TabStripModel will close the WebContents immediately.
  virtual bool RunUnloadListenerBeforeClosing(
      content::WebContents* contents) = 0;

  // Returns true if we should run unload listeners before attempts
  // to close |contents|.
  virtual bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) = 0;

  // Returns whether favicon should be shown.
  virtual bool ShouldDisplayFavicon(
      content::WebContents* web_contents) const = 0;

  // Returns whether the delegate allows reloading of WebContents.
  virtual bool CanReload() const = 0;

  // Adds the specified WebContents to read later.
  virtual void AddToReadLater(content::WebContents* web_contents) = 0;

  // Returns whether the tabstrip supports the read later feature.
  virtual bool SupportsReadLater() = 0;

  // Returns whether this tab strip model is for a web app.
  virtual bool IsForWebApp() = 0;

  // Copies the URL of the given WebContents.
  virtual void CopyURL(content::WebContents* web_contents) = 0;

  // Navigates the web_contents back to the previous page.
  virtual void GoBack(content::WebContents* web_contents) = 0;

  // Returns whether the web_contents can be navigated back.
  virtual bool CanGoBack(content::WebContents* web_contents) = 0;

  // Whether the associated window is a normal browser window.
  virtual bool IsNormalWindow() = 0;

  // Returns the BrowserWindow that owns the TabStripModel. Never changes.
  virtual BrowserWindowInterface* GetBrowserWindowInterface() = 0;

  // When performing actions to groups, some features may need to show
  // interstitials before allowing deletion. |groups| is a list of all of the
  // groups that would be Closed by the |close_callback| which may be called by
  // the implementation. This should be called with a non empty `group_ids`.
  // callback will either be executed by the delegate or asynchronously handled.
  // A bulk operation suggests that the group should be closed rather
  // than destroyed.
  virtual void OnGroupsDestruction(
      const std::vector<tab_groups::TabGroupId>& group_ids,
      base::OnceCallback<void()> close_callback,
      bool is_bulk_operation) = 0;

  virtual void OnRemovingAllTabsFromGroups(
      const std::vector<tab_groups::TabGroupId>& group_ids,
      base::OnceCallback<void()> callback) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_
