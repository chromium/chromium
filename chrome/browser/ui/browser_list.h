// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_H_

#include <stddef.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/observer_list.h"

class Browser;
class Profile;

namespace base {
class FilePath;
}

class BrowserListObserver;

// Maintains a list of Browser objects.
class BrowserList {
 public:
  using BrowserSet = base::flat_set<Browser*>;
  using BrowserVector = std::vector<Browser*>;
  using CloseCallback = base::Callback<void(const base::FilePath&)>;
  using const_iterator = BrowserVector::const_iterator;
  using const_reverse_iterator = BrowserVector::const_reverse_iterator;

  // Returns the last active browser for this list.
  Browser* GetLastActive() const;

  const_iterator begin() const { return browsers_.begin(); }
  const_iterator end() const { return browsers_.end(); }

  bool empty() const { return browsers_.empty(); }
  size_t size() const { return browsers_.size(); }

  Browser* get(size_t index) const { return browsers_[index]; }

  // Returns iterated access to list of open browsers ordered by when
  // they were last active. The underlying data structure is a vector
  // and we push_back on recent access so a reverse iterator gives the
  // latest accessed browser first.
  const_reverse_iterator begin_last_active() const {
    return last_active_browsers_.rbegin();
  }
  const_reverse_iterator end_last_active() const {
    return last_active_browsers_.rend();
  }

  // Returns the set of browsers that are currently in the closing state.
  const BrowserSet& currently_closing_browsers() const {
    return currently_closing_browsers_;
  }

  static BrowserList* GetInstance();

  // Adds or removes |browser| from the list it is associated with. The browser
  // object should be valid BEFORE these calls (for the benefit of observers),
  // so notify and THEN delete the object.
  static void AddBrowser(Browser* browser);
  static void RemoveBrowser(Browser* browser);

  // Adds and removes |observer| from the observer list for all desktops.
  // Observers are responsible for making sure the notifying browser is relevant
  // to them (e.g., on the specific desktop they care about if any).
  static void AddObserver(BrowserListObserver* observer);
  static void RemoveObserver(BrowserListObserver* observer);

  // Moves all the browsers that show on workspace |new_workspace| to the end of
  // the browser list (i.e. the browsers that were "activated" most recently).
  static void MoveBrowsersInWorkspaceToFront(const std::string& new_workspace);

  // Called by Browser objects when their window is activated (focused).  This
  // allows us to determine what the last active Browser was on each desktop.
  static void SetLastActive(Browser* browser);

  // Notifies the observers when the current active browser becomes not active.
  static void NotifyBrowserNoLongerActive(Browser* browser);

  // Notifies the observers when browser close was started. This may be called
  // more than once for a particular browser.
  static void NotifyBrowserCloseStarted(Browser* browser);

  // Closes all browsers for |profile| across all desktops.
  // TODO(mlerman): Move the Profile Deletion flow to use the overloaded
  // version of this method with a callback, then remove this method.
  static void CloseAllBrowsersWithProfile(Profile* profile);

  // Closes all browsers for |profile| across all desktops. Uses
  // TryToCloseBrowserList() to do the actual closing. Triggers any
  // OnBeforeUnload events unless |skip_beforeunload| is true. If all
  // OnBeforeUnload events are confirmed or |skip_beforeunload| is true,
  // |on_close_success| is called, otherwise |on_close_aborted| is called. Both
  // callbacks may be null.
  // Note that if there is any browser window that has been used before, the
  // user should always have a chance to save their work before closing windows
  // without triggering beforeunload events.
  static void CloseAllBrowsersWithProfile(Profile* profile,
                                          const CloseCallback& on_close_success,
                                          const CloseCallback& on_close_aborted,
                                          bool skip_beforeunload);

  // Similarly to CloseAllBrowsersWithProfile, but DCHECK's that profile is
  // Off-the-Record and doesn't close browsers with the original profile.
  static void CloseAllBrowsersWithIncognitoProfile(
      Profile* profile,
      const CloseCallback& on_close_success,
      const CloseCallback& on_close_aborted,
      bool skip_beforeunload);

  // Returns true if at least one incognito session is active across all
  // desktops.
  static bool IsIncognitoSessionActive();

  // Returns the number of active incognito sessions for |profile| across all
  // desktops. Note that this function does not count devtools windows opened
  // for incognito windows.
  // TODO(crbug.com/1014002) : Refactor the name from IncognitoSessions to
  // IncognitoBrowser here and elsewhere in this file, wherever applicable.
  static int GetIncognitoSessionsActiveForProfile(Profile* profile);

  // Returns the number of active incognito browsers except devtools windows
  // across all desktops.
  static size_t GetIncognitoBrowserCount();

  // Returns true if the incognito session for |profile| is in use in any window
  // across all desktops. This function considers devtools windows as well.
  static bool IsIncognitoSessionInUse(Profile* profile);

 private:
  BrowserList();
  ~BrowserList();

  // Helper method to remove a browser instance from a list of browsers
  static void RemoveBrowserFrom(Browser* browser, BrowserVector* browser_list);

  // Attempts to close |browsers_to_close| while respecting OnBeforeUnload
  // events. If there are no OnBeforeUnload events to be called,
  // |on_close_success| will be called, with a parameter of |profile_path|,
  // and the Browsers will then be closed. If at least one unfired
  // OnBeforeUnload event is found, handle it with a callback to
  // PostTryToCloseBrowserWindow, which upon success will recursively call this
  // method to handle any other OnBeforeUnload events. If aborted in the
  // OnBeforeUnload event, PostTryToCloseBrowserWindow will call
  // |on_close_aborted| instead and reset all OnBeforeUnload event handlers.
  static void TryToCloseBrowserList(const BrowserVector& browsers_to_close,
                                    const CloseCallback& on_close_success,
                                    const CloseCallback& on_close_aborted,
                                    const base::FilePath& profile_path,
                                    const bool skip_beforeunload);

  // Called after handling an OnBeforeUnload event. If |tab_close_confirmed| is
  // true, calls |TryToCloseBrowserList()|, passing the parameters
  // |browsers_to_close|, |on_close_success|, |on_close_aborted|, and
  // |profile_path|. Otherwise, resets all the OnBeforeUnload event handlers and
  // calls |on_close_aborted|.
  static void PostTryToCloseBrowserWindow(
      const BrowserVector& browsers_to_close,
      const CloseCallback& on_close_success,
      const CloseCallback& on_close_aborted,
      const base::FilePath& profile_path,
      const bool skip_beforeunload,
      bool tab_close_confirmed);

  // A vector of the browsers in this list, in the order they were added.
  BrowserVector browsers_;
  // A vector of the browsers in this list that have been activated, in the
  // reverse order in which they were last activated.
  BrowserVector last_active_browsers_;
  // A vector of the browsers that are currently in the closing state.
  BrowserSet currently_closing_browsers_;

  // A list of observers which will be notified of every browser addition and
  // removal across all BrowserLists.
  static base::LazyInstance<
      base::ObserverList<BrowserListObserver>::Unchecked>::Leaky observers_;

  static BrowserList* instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserList);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_H_
