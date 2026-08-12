// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_UI_CONTROLLER_BROWSER_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_UI_CONTROLLER_BROWSER_UI_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BookmarkBarController;
class BrowserWindow;
class BrowserWindowInterface;
class StatusBubble;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// Controller responsible for browser UI updates, UI update coalescing,
// toolbar state updates, and tab UI notification dispatch.
class BrowserUiController {
 public:
  DECLARE_USER_DATA(BrowserUiController);

  BrowserUiController(BrowserWindowInterface& browser,
                      TabStripModel& tab_strip_model,
                      BrowserWindow& window,
                      BookmarkBarController& bookmark_bar_controller);
  BrowserUiController(const BrowserUiController&) = delete;
  BrowserUiController& operator=(const BrowserUiController&) = delete;
  ~BrowserUiController();

  static BrowserUiController* From(BrowserWindowInterface* browser);
  static const BrowserUiController* From(const BrowserWindowInterface* browser);

  // Called by Navigate() when a navigation has occurred in a tab in
  // this Browser. Updates the UI for the start of this navigation.
  void UpdateUIForNavigationInTab(content::WebContents* contents,
                                  ui::PageTransition transition,
                                  NavigateParams::WindowAction action,
                                  bool user_initiated);

  // Does one or both of the following for each bit in |changed_flags|:
  // . If the update should be processed immediately, it is.
  // . If the update should processed asynchronously (to avoid lots of ui
  //   updates), then scheduled_updates_ is updated for the |source| and update
  //   pair and a task is scheduled (assuming it isn't running already)
  //   that invokes ProcessPendingUIUpdates.
  void ScheduleUIUpdate(content::WebContents* source, unsigned changed_flags);

  // Processes all pending updates to the UI that have been scheduled by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(content::WebContents* contents);

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Asks the toolbar to layout and redraw to reflect the current security
  // state.
  void UpdateToolbarSecurityState();

  // Notifies the tab UI that it should update when the browser schedule or
  // process UI updates.
  void NotifyTabUIChanged(tabs::TabInterface* tab, TabChangeType change_type);

  // Returns the list of StatusBubbles from the current toolbar. It is possible
  // for this to be empty if called before the toolbar has initialized. In a
  // split view, there will be multiple status bubbles with the active one
  // listed first.
  std::vector<StatusBubble*> GetStatusBubbles();

  // Updates the loading state for the window and tabstrip.
  void UpdateWindowForLoadingStateChanged(content::WebContents* source,
                                          bool should_show_loading_ui);

  void set_update_ui_immediately_for_testing(bool immediate = true) {
    update_ui_immediately_for_testing_ = immediate;
  }

 private:
  using UpdateMap = std::map<tabs::TabInterface*, int>;

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<TabStripModel> tab_strip_model_;
  const raw_ref<BrowserWindow> window_;
  const raw_ref<BookmarkBarController> bookmark_bar_controller_;

  // Maps TabInterface to pending UI update invalidate flags.
  UpdateMap scheduled_updates_;

  // If true, immediately updates the UI when scheduled.
  bool update_ui_immediately_for_testing_ = false;

  ui::ScopedUnownedUserData<BrowserUiController> scoped_unowned_user_data_;

  // The following factory is used for chrome update coalescing.
  base::WeakPtrFactory<BrowserUiController> chrome_updater_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BROWSER_UI_CONTROLLER_BROWSER_UI_CONTROLLER_H_
