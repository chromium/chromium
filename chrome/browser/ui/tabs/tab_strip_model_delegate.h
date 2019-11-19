// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"

class Browser;
class GURL;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
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
  virtual void AddTabAt(const GURL& url,
                        int index,
                        bool foreground,
                        base::Optional<TabGroupId> group = base::nullopt) = 0;

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
    ~NewStripContents();
    NewStripContents(NewStripContents&&);
    // The WebContents to add.
    std::unique_ptr<content::WebContents> web_contents;
    // A bitmask of TabStripModel::AddTabTypes to apply to the added contents.
    int add_types = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(NewStripContents);
  };
  virtual Browser* CreateNewStripWithContents(
      std::vector<NewStripContents> contentses,
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

  // Duplicates the contents at the provided index and places it into its own
  // window.
  virtual void DuplicateContentsAt(int index) = 0;

  // Creates an entry in the historical tab database for the specified
  // WebContents.
  virtual void CreateHistoricalTab(content::WebContents* contents) = 0;

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
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_DELEGATE_H_
