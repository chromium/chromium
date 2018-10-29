// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

class Profile;
class TabStripModelDelegate;

namespace content {
class WebContents;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
// A model & low level controller of a Browser Window tabstrip. Holds a vector
// of WebContentses, and provides an API for adding, removing and
// shuffling them, as well as a higher level API for doing specific Browser-
// related tasks like adding new Tabs from just a URL, etc.
//
// Each tab may be pinned. Pinned tabs are locked to the left side of the tab
// strip and rendered differently (small tabs with only a favicon). The model
// makes sure all pinned tabs are at the beginning of the tab strip. For
// example, if a non-pinned tab is added it is forced to be with non-pinned
// tabs. Requests to move tabs outside the range of the tab type are ignored.
// For example, a request to move a pinned tab after non-pinned tabs is ignored.
//
// A TabStripModel has one delegate that it relies on to perform certain tasks
// like creating new TabStripModels (probably hosted in Browser windows) when
// required. See TabStripDelegate above for more information.
//
// A TabStripModel also has N observers (see TabStripModelObserver above),
// which can be registered via Add/RemoveObserver. An Observer is notified of
// tab creations, removals, moves, and other interesting events. The
// TabStrip implements this interface to know when to create new tabs in
// the View, and the Browser object likewise implements to be able to update
// its bookkeeping when such events happen.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel {
 public:
  // Used to specify what should happen when the tab is closed.
  enum CloseTypes {
    CLOSE_NONE                     = 0,

    // Indicates the tab was closed by the user. If true,
    // WebContents::SetClosedByUserGesture(true) is invoked.
    CLOSE_USER_GESTURE             = 1 << 0,

    // If true the history is recorded so that the tab can be reopened later.
    // You almost always want to set this.
    CLOSE_CREATE_HISTORICAL_TAB    = 1 << 1,
  };

  // Constants used when adding tabs.
  enum AddTabTypes {
    // Used to indicate nothing special should happen to the newly inserted tab.
    ADD_NONE = 0,

    // The tab should be active.
    ADD_ACTIVE = 1 << 0,

    // The tab should be pinned.
    ADD_PINNED = 1 << 1,

    // If not set the insertion index of the WebContents is left up to the Order
    // Controller associated, so the final insertion index may differ from the
    // specified index. Otherwise the index supplied is used.
    ADD_FORCE_INDEX = 1 << 2,

    // If set the newly inserted tab's opener is set to the active tab. If not
    // set the tab may still inherit the opener under certain situations.
    ADD_INHERIT_OPENER = 1 << 3,
  };

  // Enumerates different ways to open a new tab. Does not apply to opening
  // existing links or searches in a new tab, only to brand new empty tabs.
  enum NewTab {
    // New tab was opened using the new tab button on the tab strip.
    NEW_TAB_BUTTON,

    // New tab was opened using the menu command - either through the keyboard
    // shortcut, or by opening the menu and selecting the command. Applies to
    // both app menu and the menu bar's File menu (on platforms that have one).
    NEW_TAB_COMMAND,

    // New tab was opened through the context menu on the tab strip.
    NEW_TAB_CONTEXT_MENU,

    // Number of enum entries, used for UMA histogram reporting macros.
    NEW_TAB_ENUM_COUNT,
  };

  static const int kNoTab = -1;

  // Construct a TabStripModel with a delegate to help it do certain things
  // (see the TabStripModelDelegate documentation). |delegate| cannot be NULL.
  explicit TabStripModel(TabStripModelDelegate* delegate, Profile* profile);
  ~TabStripModel();

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of WebContentses/emptiness of the TabStripModel.
  int count() const { return static_cast<int>(contents_data_.size()); }
  bool empty() const { return contents_data_.empty(); }

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const { return profile_; }

  // Retrieve the index of the currently active WebContents. This will be
  // ui::ListSelectionModel::kUnselectedIndex if no tab is currently selected
  // (this happens while the tab strip is being initialized or is empty).
  int active_index() const { return selection_model_.active(); }

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  bool closing_all() const { return closing_all_; }

  // Basic API /////////////////////////////////////////////////////////////////

  // Determines if the specified index is contained within the TabStripModel.
  bool ContainsIndex(int index) const;

  // Adds the specified WebContents in the default location. Tabs opened
  // in the foreground inherit the opener of the previously active tab.
  void AppendWebContents(std::unique_ptr<content::WebContents> contents,
                         bool foreground);

  // Adds the specified WebContents at the specified location.
  // |add_types| is a bitmask of AddTabTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddTabTypes is meaningless here.  The only time
  // the |index| is changed is if using the index would result in breaking the
  // constraint that all pinned tabs occur before non-pinned tabs.
  // See also AddWebContents.
  void InsertWebContentsAt(int index,
                           std::unique_ptr<content::WebContents> contents,
                           int add_types);

  // Closes the WebContents at the specified index. This causes the
  // WebContents to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes. Returns true if the
  // WebContents was closed immediately, false if it was not closed (we
  // may be waiting for a response from an onunload handler, or waiting for the
  // user to confirm closure).
  bool CloseWebContentsAt(int index, uint32_t close_types);

  // Replaces the WebContents at |index| with |new_contents|. The
  // WebContents that was at |index| is returned and its ownership returns
  // to the caller.
  std::unique_ptr<content::WebContents> ReplaceWebContentsAt(
      int index,
      std::unique_ptr<content::WebContents> new_contents);

  // Detaches the WebContents at the specified index from this strip. The
  // WebContents is not destroyed, just removed from display. The caller
  // is responsible for doing something with it (e.g. stuffing it into another
  // strip). Returns the detached WebContents.
  std::unique_ptr<content::WebContents> DetachWebContentsAt(int index);

  // Makes the tab at the specified index the active tab. |user_gesture| is true
  // if the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was activated as a by-product of some other
  // action.
  void ActivateTabAt(int index, bool user_gesture);

  // Move the WebContents at the specified index to another index. This
  // method does NOT send Detached/Attached notifications, rather it moves the
  // WebContents inline and sends a Moved notification instead.
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but its index may have incremented or decremented
  // one slot.
  void MoveWebContentsAt(int index, int to_position, bool select_after_move);

  // Moves the selected tabs to |index|. |index| is treated as if the tab strip
  // did not contain any of the selected tabs. For example, if the tabstrip
  // contains [A b c D E f] (upper case selected) and this is invoked with 1 the
  // result is [b A D E c f].
  // This method maintains that all pinned tabs occur before non-pinned tabs.
  // When pinned tabs are selected the move is processed in two chunks: first
  // pinned tabs are moved, then non-pinned tabs are moved. If the index is
  // after (pinned-tab-count - selected-pinned-tab-count), then the index the
  // non-pinned selected tabs are moved to is (index +
  // selected-pinned-tab-count). For example, if the model consists of
  // [A b c D E f] (A b c are pinned) and this is invoked with 2, the result is
  // [b c A D E f]. In this example nothing special happened because the target
  // index was <= (pinned-tab-count - selected-pinned-tab-count). If the target
  // index were 3, then the result would be [b c A f D F]. A, being pinned, can
  // move no further than index 2. The non-pinned tabs are moved to the target
  // index + selected-pinned tab-count (3 + 1).
  void MoveSelectedTabsTo(int index);

  // Returns the currently active WebContents, or NULL if there is none.
  content::WebContents* GetActiveWebContents() const;

  // Returns the WebContents at the specified index, or NULL if there is
  // none.
  content::WebContents* GetWebContentsAt(int index) const;

  // Returns the index of the specified WebContents, or TabStripModel::kNoTab
  // if the WebContents is not in this TabStripModel.
  int GetIndexOfWebContents(const content::WebContents* contents) const;

  // Notify any observers that the WebContents at the specified index has
  // changed in some way. See TabChangeType for details of |change_type|.
  void UpdateWebContentsStateAt(int index, TabChangeType change_type);

  // Cause a tab to display a UI indication to the user that it needs their
  // attention.
  void SetTabNeedsAttentionAt(int index, bool attention);

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  void CloseAllTabs();

  // Returns true if there are any WebContentses that are currently loading.
  bool TabsAreLoading() const;

  // Returns the WebContents that opened the WebContents at |index|, or NULL if
  // there is no opener on record.
  content::WebContents* GetOpenerOfWebContentsAt(int index);

  // Changes the |opener| of the WebContents at |index|.
  // Note: |opener| must be in this tab strip.
  void SetOpenerOfWebContentsAt(int index, content::WebContents* opener);

  // Returns the index of the last WebContents in the model opened by the
  // specified opener, starting at |start_index|.
  int GetIndexOfLastWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index) const;

  // To be called when a navigation is about to occur in the specified
  // WebContents. Depending on the tab, and the transition type of the
  // navigation, the TabStripModel may adjust its selection behavior and opener
  // inheritance.
  void TabNavigating(content::WebContents* contents,
                     ui::PageTransition transition);

  // Changes the blocked state of the tab at |index|.
  void SetTabBlocked(int index, bool blocked);

  // Changes the pinned state of the tab at |index|. See description above
  // class for details on this.
  void SetTabPinned(int index, bool pinned);

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  bool IsTabPinned(int index) const;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  bool IsTabBlocked(int index) const;

  // Returns the index of the first tab that is not a pinned tab. This returns
  // |count()| if all of the tabs are pinned tabs, and 0 if none of the tabs are
  // pinned tabs.
  int IndexOfFirstNonPinnedTab() const;

  // Extends the selection from the anchor to |index|.
  void ExtendSelectionTo(int index);

  // Toggles the selection at |index|. This does nothing if |index| is selected
  // and there are no other selected tabs.
  void ToggleSelectionAt(int index);

  // Makes sure the tabs from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(int index);

  // Returns true if the tab at |index| is selected.
  bool IsTabSelected(int index) const;

  // Sets the selection to match that of |source|.
  void SetSelectionFromModel(ui::ListSelectionModel source);

  const ui::ListSelectionModel& selection_model() const;

  // Command level API /////////////////////////////////////////////////////////

  // Adds a WebContents at the best position in the TabStripModel given
  // the specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTabTypes; see it for details. This method ends up calling into
  // InsertWebContentsAt to do the actual insertion. Pass kNoTab for |index| to
  // append the contents to the end of the tab strip.
  void AddWebContents(std::unique_ptr<content::WebContents> contents,
                      int index,
                      ui::PageTransition transition,
                      int add_types);

  // Closes the selected tabs.
  void CloseSelectedTabs();

  // Select adjacent tabs
  void SelectNextTab();
  void SelectPreviousTab();

  // Selects the last tab in the tab strip.
  void SelectLastTab();

  // Swap adjacent tabs.
  void MoveTabNext();
  void MoveTabPrevious();

  // View API //////////////////////////////////////////////////////////////////

  // Context menu functions.
  enum ContextMenuCommand {
    CommandFirst,
    CommandNewTab,
    CommandReload,
    CommandDuplicate,
    CommandCloseTab,
    CommandCloseOtherTabs,
    CommandCloseTabsToRight,
    CommandRestoreTab,
    CommandTogglePinned,
    CommandToggleTabAudioMuted,
    CommandToggleSiteMuted,
    CommandBookmarkAllTabs,
    CommandLast
  };

  // Returns true if the specified command is enabled. If |context_index| is
  // selected the response applies to all selected tabs.
  bool IsContextMenuCommandEnabled(int context_index,
                                   ContextMenuCommand command_id) const;

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.  If |context_index| is selected the
  // command applies to all selected tabs.
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id);

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  std::vector<int> GetIndicesClosedByCommand(int index,
                                             ContextMenuCommand id) const;

  // Returns true if 'CommandToggleTabAudioMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuMute(int index);

  // Returns true if 'CommandToggleSiteMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuMuteSites(int index);

  // Returns true if 'CommandTogglePinned' will pin. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuPin(int index);

  // Convert a ContextMenuCommand into a browser command. Returns true if a
  // corresponding browser command exists, false otherwise.
  static bool ContextMenuCommandToBrowserCommand(int cmd_id, int* browser_cmd);

  // Access the order controller. Exposed only for unit tests.
  TabStripModelOrderController* order_controller() const {
    return order_controller_.get();
  }

  // Returns the index of the next WebContents in the sequence of WebContentses
  // spawned by the specified WebContents after |start_index|.
  int GetIndexOfNextWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index) const;

  // Forget all opener relationships, to reduce unpredictable tab switching
  // behavior in complex session states. The exact circumstances under which
  // this method is called are left up to TabStripModelOrderController.
  void ForgetAllOpeners();

  // Forgets the opener relationship of the specified WebContents.
  void ForgetOpener(content::WebContents* contents);

  // Returns true if the opener relationships present for |contents| should be
  // reset when _any_ active tab change occurs (rather than just one outside the
  // current tree of openers).
  bool ShouldResetOpenerOnActiveTabChange(content::WebContents* contents) const;

 private:
  class WebContentsData;
  struct DetachedWebContents;
  struct DetachNotifications;

  // Performs all the work to detach a WebContents instance but avoids sending
  // most notifications. TabClosingAt() and TabDetachedAt() are sent because
  // observers are reliant on the selection model being accurate at the time
  // that TabDetachedAt() is called.
  std::unique_ptr<content::WebContents> DetachWebContentsImpl(
      int index,
      bool create_historical_tab,
      bool will_delete);

  // We batch send notifications. This has two benefits:
  //   1) This allows us to send the minimal number of necessary notifications.
  //   This is important because some notifications cause the main thread to
  //   synchronously communicate with the GPU process and cause jank.
  //   https://crbug.com/826287.
  //   2) This allows us to avoid some problems caused by re-entrancy [e.g.
  //   using destroyed WebContents instances]. Ideally, this second check
  //   wouldn't be necessary because we would enforce that there is no
  //   re-entrancy in the TabStripModel, but that condition is currently
  //   violated in tests [and possibly in the wild as well].
  void SendDetachWebContentsNotifications(DetachNotifications* notifications);

  bool RunUnloadListenerBeforeClosing(content::WebContents* contents);
  bool ShouldRunUnloadListenerBeforeClosing(content::WebContents* contents);

  int ConstrainInsertionIndex(int index, bool pinned_tab);

  // If |index| is selected all the selected indices are returned, otherwise a
  // vector with |index| is returned. This is used when executing commands to
  // determine which indices the command applies to.
  std::vector<int> GetIndicesForCommand(int index) const;

  // Returns true if the specified WebContents is a New Tab at the end of
  // the tabstrip. We check for this because opener relationships are _not_
  // forgotten for the New Tab page opened as a result of a New Tab gesture
  // (e.g. Ctrl+T, etc) since the user may open a tab transiently to look up
  // something related to their current activity.
  bool IsNewTabAtEndOfTabStrip(content::WebContents* contents) const;

  // Closes the WebContentses at the specified indices. This causes the
  // WebContentses to be destroyed, but it may not happen immediately. If
  // the page in question has an unload event the WebContents will not be
  // destroyed until after the event has completed, which will then call back
  // into this method.
  //
  // Returns true if the WebContentses were closed immediately, false if we
  // are waiting for the result of an onunload handler.
  bool InternalCloseTabs(base::span<content::WebContents* const> items,
                         uint32_t close_types);

  // |close_types| is a bitmask of the types in CloseTypes.
  // Returns true if all the tabs have been deleted. A return value of false
  // means some portion (potentially none) of the WebContents were deleted.
  // WebContents not deleted by this function are processing unload handlers
  // which may eventually be deleted based on the results of the unload handler.
  // Additionally processing the unload handlers may result in needing to show
  // UI for the WebContents. See UnloadController for details on how unload
  // handlers are processed.
  bool CloseWebContentses(base::span<content::WebContents* const> items,
                          uint32_t close_types);

  // Gets the WebContents at an index. Does no bounds checking.
  content::WebContents* GetWebContentsAtImpl(int index) const;

  // Returns the WebContentses at the specified indices. This does no checking
  // of the indices, it is assumed they are valid.
  std::vector<content::WebContents*> GetWebContentsesByIndices(
      const std::vector<int>& indices);

  // Notifies the observers if the active tab has changed.
  void NotifyIfActiveTabChanged(const TabStripSelectionChange& selection);

  // Notifies the observers if the active tab or the tab selection has changed.
  // |old_model| is a snapshot of |selection_model_| before the change.
  // Note: This function might end up sending 0 to 2 notifications in the
  // following order: ActiveTabChanged, TabSelectionChanged.
  void NotifyIfActiveOrSelectionChanged(
      const TabStripSelectionChange& selection);

  // Sets the selection to |new_model| and notifies any observers.
  // Note: This function might end up sending 0 to 3 notifications in the
  // following order: TabDeactivated, ActiveTabChanged, TabSelectionChanged.
  // |selection| will be filled with information corresponding to 3 notification
  // above. When it's |triggered_by_other_operation|, This won't notify
  // observers that selection was changed. Callers should notify it by
  // themselves.
  TabStripSelectionChange SetSelection(
      ui::ListSelectionModel new_model,
      TabStripModelObserver::ChangeReason reason,
      bool triggered_by_other_operation);

  // Selects either the next tab (|forward| is true), or the previous tab
  // (|forward| is false).
  void SelectRelativeTab(bool forward);

  // Does the work of MoveWebContentsAt. This has no checks to make sure the
  // position is valid, those are done in MoveWebContentsAt.
  void MoveWebContentsAtImpl(int index,
                             int to_position,
                             bool select_after_move);

  // Implementation of MoveSelectedTabsTo. Moves |length| of the selected tabs
  // starting at |start| to |index|. See MoveSelectedTabsTo for more details.
  void MoveSelectedTabsToImpl(int index, size_t start, size_t length);

  // Sets the sound content setting for each site at the |indices|.
  void SetSitesMuted(const std::vector<int>& indices, bool mute) const;

  // Sets the opener of any tabs that reference the tab at |index| to that tab's
  // opener.
  void FixOpeners(int index);

  // The WebContents data currently hosted within this TabStripModel. This must
  // be kept in sync with |selection_model_|.
  std::vector<std::unique_ptr<WebContentsData>> contents_data_;

  TabStripModelDelegate* delegate_;
  base::ObserverList<TabStripModelObserver>::Unchecked observers_;

  // A profile associated with this TabStripModel.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_ = false;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  std::unique_ptr<TabStripModelOrderController> order_controller_;

  // This must be kept in sync with |contents_data_|.
  ui::ListSelectionModel selection_model_;

  // TabStripModel is not re-entrancy safe. This member is used to guard public
  // methods that mutate state of |selection_model_| or |contents_data_|.
  bool reentrancy_guard_ = false;

  base::WeakPtrFactory<TabStripModel> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TabStripModel);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
