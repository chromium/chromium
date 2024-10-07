// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_scrubbing_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Profile;
class TabContentsData;
class TabGroupModel;
class TabStripModelDelegate;
class TabStripModelObserver;
class TabDragController;

namespace content {
class WebContents;
}

class TabGroupModelFactory {
 public:
  TabGroupModelFactory();
  TabGroupModelFactory(const TabGroupModelFactory&) = delete;
  TabGroupModelFactory& operator=(const TabGroupModelFactory&) = delete;

  static TabGroupModelFactory* GetInstance();
  std::unique_ptr<TabGroupModel> Create(TabGroupController* controller);
};

// Holds state for a WebContents that has been detached from the tab strip.
// Will also handle WebContents deletion if |remove_reason| is kDeleted, or
// WebContents caching if |remove_reason| is kCached.
struct DetachedWebContents {
  DetachedWebContents(int index_before_any_removals,
                      int index_at_time_of_removal,
                      std::unique_ptr<tabs::TabModel> tab,
                      content::WebContents* contents,
                      TabStripModelChange::RemoveReason remove_reason,
                      std::optional<SessionID> id);
  DetachedWebContents(const DetachedWebContents&) = delete;
  DetachedWebContents& operator=(const DetachedWebContents&) = delete;
  ~DetachedWebContents();
  DetachedWebContents(DetachedWebContents&&);

  // When a WebContents is removed the delegate is given a chance to
  // take ownership of it (generally for caching). If the delegate takes
  // ownership, `tab` will be null, and `contents` will be
  // non-null. In other words, all observers should use `contents`, it is
  // guaranteed to be valid for the life time of the notification (and
  // possibly longer).
  std::unique_ptr<tabs::TabModel> tab;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> contents;

  // The index of the WebContents in the original selection model of the tab
  // strip [prior to any tabs being removed, if multiple tabs are being
  // simultaneously removed].
  const int index_before_any_removals;

  // The index of the WebContents at the time it is being removed. If multiple
  // tabs are being simultaneously removed, the index reflects previously
  // removed tabs in this batch.
  const int index_at_time_of_removal;

  TabStripModelChange::RemoveReason remove_reason;

  // The |contents| associated optional SessionID, used as key for
  // ClosedTabCache. We only cache |contents| if |remove_reason| is kCached.
  std::optional<SessionID> id;
};

// A feature which wants to show tabstrip-modal UI should call
// TabStripController::ShowModalUI and keep alive the instance of
// ScopedTabStripModalUI for the duration of the tabstrip-modal UI.
class ScopedTabStripModalUI {
 public:
  ScopedTabStripModalUI() = default;
  virtual ~ScopedTabStripModalUI() = default;
};

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
// A model & low level controller of a Browser Window tabstrip. Holds a vector
// of WebContents, and provides an API for adding, removing and
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
// This implementation of TabStripModel is not thread-safe and should only be
// accessed on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel : public TabGroupController {
 public:
  // TODO(crbug.com/40881446): Remove this, and use std::optional<size_t> (or at
  // least std::optional<int>) in its place.
  static constexpr int kNoTab = -1;

  TabStripModel() = delete;

  // Construct a TabStripModel with a delegate to help it do certain things
  // (see the TabStripModelDelegate documentation). |delegate| cannot be NULL.
  // the TabGroupModelFactory can be replaced with a nullptr to set the
  // group_model to null in cases where groups are not supported.
  explicit TabStripModel(TabStripModelDelegate* delegate,
                         Profile* profile,
                         TabGroupModelFactory* group_model_factory =
                             TabGroupModelFactory::GetInstance());

  TabStripModel(const TabStripModel&) = delete;
  TabStripModel& operator=(const TabStripModel&) = delete;

  ~TabStripModel() override;

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Sets the TabStripModelObserver used by the UI showing the tabs. As other
  // observers may query the UI for state, the UI's observer must be first.
  void SetTabStripUI(TabStripModelObserver* observer);

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of WebContentses/emptiness of the TabStripModel.
  int count() const;
  bool empty() const;

  int GetIndexOfTab(tabs::TabHandle tab) const;
  tabs::TabHandle GetTabHandleAt(int index) const;

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const { return profile_; }

  // Retrieve the index of the currently active WebContents. The only time this
  // is kNoTab is if the tab strip is being initialized or destroyed. Note that
  // tab strip destruction is an asynchronous process.
  int active_index() const {
    return selection_model_.active().has_value()
               ? static_cast<int>(selection_model_.active().value())
               : kNoTab;
  }

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
  // Use of the detached tab is preferred over webcontents, so when possible
  // use AppendTab instead of this method.
  void AppendWebContents(std::unique_ptr<content::WebContents> contents,
                         bool foreground);

  // Adds the specified Tab at the end of the Tabstrip. Tabs opened
  // in the foreground inherit the opener of the previously active tab and
  // become the active tab.
  void AppendTab(std::unique_ptr<tabs::TabModel> tab, bool foreground);

  // Adds the specified WebContents at the specified location.
  // |add_types| is a bitmask of AddTabTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddTabTypes is meaningless here. The only time
  // the |index| is changed is if using the index would result in breaking the
  // constraint that all pinned tabs occur before non-pinned tabs. It returns
  // the index the web contents is actually inserted to. See also
  // AddWebContents.
  int InsertWebContentsAt(
      int index,
      std::unique_ptr<content::WebContents> contents,
      int add_types,
      std::optional<tab_groups::TabGroupId> group = std::nullopt);

  // Adds a TabModel from another tabstrip at the specified location. See
  // InsertWebContentsAt.
  int InsertDetachedTabAt(
      int index,
      std::unique_ptr<tabs::TabModel> tab,
      int add_types,
      std::optional<tab_groups::TabGroupId> group = std::nullopt);

  // Closes the WebContents at the specified index. This causes the
  // WebContents to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes.
  void CloseWebContentsAt(int index, uint32_t close_types);

  // Discards the WebContents at |index| and replaces it with |new_contents|.
  // The WebContents that was at |index| is returned and its ownership returns
  // to the caller.
  std::unique_ptr<content::WebContents> DiscardWebContentsAt(
      int index,
      std::unique_ptr<content::WebContents> new_contents);

  // Detaches the tab at the specified index for reinsertion into another tab
  // strip. Returns the detached tab.
  std::unique_ptr<tabs::TabModel> DetachTabAtForInsertion(int index);

  // Detaches the WebContents at the specified index and immediately deletes it.
  void DetachAndDeleteWebContentsAt(int index);

  // Makes the tab at the specified index the active tab. |gesture_detail.type|
  // contains the gesture type that triggers the tab activation.
  // |gesture_detail.time_stamp| contains the timestamp of the user gesture, if
  // any.
  void ActivateTabAt(
      int index,
      TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kNone));

  // Move the WebContents at the specified index to another index. This
  // method does NOT send Detached/Attached notifications, rather it moves the
  // WebContents inline and sends a Moved notification instead.
  // EnsureGroupContiguity() is called after the move, so this will never result
  // in non-contiguous group (though the moved tab's group may change).
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but its index may have incremented or decremented
  // one slot. It returns the index the web contents is actually moved to.
  int MoveWebContentsAt(int index, int to_position, bool select_after_move);

  // This is similar to `MoveWebContentsAt` but takes in an additional `group`
  // parameter that the tab is assigned to with the move. This does not make a
  // best case effort to ensure group contiguity and would rather CHECK if it
  // breaks group contiguity.
  int MoveWebContentsAt(int index,
                        int to_position,
                        bool select_after_move,
                        std::optional<tab_groups::TabGroupId> group);

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
  void MoveSelectedTabsTo(int index,
                          std::optional<tab_groups::TabGroupId> group);

  // Moves all tabs in |group| to |to_index|. This has no checks to make sure
  // the position is valid for a group to move to.
  void MoveGroupTo(const tab_groups::TabGroupId& group, int to_index);

  // Returns the currently active WebContents, or NULL if there is none.
  content::WebContents* GetActiveWebContents() const;

  // Returns the currently active Tab, or NULL if there is none.
  tabs::TabModel* GetActiveTab() const;

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

  // Close all tabs in the given |group| at once.
  void CloseAllTabsInGroup(const tab_groups::TabGroupId& group);

  // Returns true if there are any WebContentses that are currently loading.
  bool TabsAreLoading() const;

  // Returns the WebContents that opened the WebContents at |index|, or NULL if
  // there is no opener on record.
  tabs::TabModel* GetOpenerOfTabAt(const int index) const;

  // Changes the |opener| of the WebContents at |index|.
  // Note: |opener| must be in this tab strip. Also a tab must not be its own
  // opener.
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

  // Changes the pinned state of the tab at `index`. See description above
  // class for details on this. Returns the index the tab is now at (it may have
  // been moved to maintain contiguity of pinned tabs at the beginning of the
  // tabstrip).)
  int SetTabPinned(int index, bool pinned);

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  bool IsTabPinned(int index) const;

  bool IsTabCollapsed(int index) const;

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  bool IsTabBlocked(int index) const;

  // Returns true if the tab at |index| is allowed to be closed.
  bool IsTabClosable(int index) const;

  // Returns true if the tab corresponding to |contents| is allowed to be
  // closed.
  bool IsTabClosable(const content::WebContents* contents) const;

  // Returns the group that contains the tab at |index|, or nullopt if the tab
  // index is invalid or not grouped.
  std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;

  // If a tab inserted at |index| would be within a tab group, return that
  // group's ID. Otherwise, return nullopt. If |index| points to the first tab
  // in a group, it will return nullopt since a new tab would be either between
  // two different groups or just after a non-grouped tab.
  std::optional<tab_groups::TabGroupId> GetSurroundingTabGroup(int index) const;

  // Returns the index of the first tab that is not a pinned tab. This returns
  // |count()| if all of the tabs are pinned tabs, and 0 if none of the tabs are
  // pinned tabs.
  int IndexOfFirstNonPinnedTab() const;

  // Extends the selection from the anchor to |index|.
  void ExtendSelectionTo(int index);

  // Returns true if the selection was toggled; this can fail if the tabstrip
  // is not editable.
  bool ToggleSelectionAt(int index);

  // Makes sure the tabs from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(int index);

  // Returns true if the tab at |index| is selected.
  bool IsTabSelected(int index) const;

  // Sets the selection to match that of |source|.
  void SetSelectionFromModel(ui::ListSelectionModel source);

  const ui::ListSelectionModel& selection_model() const;

  // Features that want to show tabstrip-modal UI are mutually exclusive.
  // Before showing a modal UI first check `CanShowModalUI`. Then call
  // ShowModalUI() and keep `ScopedTabStripModal` alive to prevent other
  // features from showing tabstrip-modal UI.
  bool CanShowModalUI() const;
  std::unique_ptr<ScopedTabStripModalUI> ShowModalUI();
  void ForceShowingModalUIForTesting(bool showing);

  // Command level API /////////////////////////////////////////////////////////

  // Adds a WebContents at the best position in the TabStripModel given
  // the specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTabTypes; see it for details. This method ends up calling into
  // InsertWebContentsAt to do the actual insertion. Pass kNoTab for |index| to
  // append the contents to the end of the tab strip.
  void AddWebContents(
      std::unique_ptr<content::WebContents> contents,
      int index,
      ui::PageTransition transition,
      int add_types,
      std::optional<tab_groups::TabGroupId> group = std::nullopt);
  void AddTab(std::unique_ptr<tabs::TabModel> tab,
              int index,
              ui::PageTransition transition,
              int add_types,
              std::optional<tab_groups::TabGroupId> group = std::nullopt);

  // Closes the selected tabs.
  void CloseSelectedTabs();

  // Select adjacent tabs
  void SelectNextTab(
      TabStripUserGestureDetails detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));
  void SelectPreviousTab(
      TabStripUserGestureDetails detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Selects the last tab in the tab strip.
  void SelectLastTab(
      TabStripUserGestureDetails detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Moves the active in the specified direction. Respects group boundaries.
  void MoveTabNext();
  void MoveTabPrevious();

  // This is used by the `tab_drag_controller` to help precompute the group the
  // selected tabs would be a part of if they moved to a destination index. It
  // returns the index of the tabs in the current model that would end up being
  // the adjacent tabs of the selected unpinned tabs post move operation.
  std::pair<std::optional<int>, std::optional<int>>
  GetAdjacentTabsAfterSelectedMove(base::PassKey<TabDragController>,
                                   int destination_index);

  // Create a new tab group and add the set of tabs pointed to be |indices| to
  // it. Pins all of the tabs if any of them were pinned, and reorders the tabs
  // so they are contiguous and do not split an existing group in half. Returns
  // the new group. |indices| must be sorted in ascending order.
  tab_groups::TabGroupId AddToNewGroup(const std::vector<int> indices);

  // Creates a new group from an `group_id` and `visual_data`. This is used in
  // cases to re-create a deleted group like restore and tab dragging. All the
  // tabs at `indices` are moved to the group created.
  tab_groups::TabGroupId AddToNewGroup(
      const std::vector<int> indices,
      const tab_groups::TabGroupId group_id,
      tab_groups::TabGroupVisualData visual_data);

  // Add the set of tabs pointed to by |indices| to the given tab group |group|.
  // The tabs take on the pinnedness of the tabs already in the group. Tabs
  // before the group will move to the start, while tabs after the group will
  // move to the end. If |add_to_end| is true, all tabs will instead move to
  // the end. |indices| must be sorted in ascending order.
  void AddToExistingGroup(const std::vector<int> indices,
                          const tab_groups::TabGroupId group,
                          const bool add_to_end = false);

  // Similar to AddToExistingGroup(), but creates a group with id |group| if it
  // doesn't exist. This is only intended to be called from session restore
  // code.
  void AddToGroupForRestore(const std::vector<int>& indices,
                            const tab_groups::TabGroupId& group);

  // Removes the set of tabs pointed to by |indices| from the the groups they
  // are in, if any. The tabs are moved out of the group if necessary. |indices|
  // must be sorted in ascending order.
  void RemoveFromGroup(const std::vector<int>& indices);

  TabGroupModel* group_model() const { return group_model_.get(); }

  bool SupportsTabGroups() const { return group_model_.get() != nullptr; }

  // Returns true if one or more of the tabs pointed to by |indices| are
  // supported by read later.
  bool IsReadLaterSupportedForAny(const std::vector<int>& indices);

  // Saves tabs with url supported by Read Later.
  void AddToReadLater(const std::vector<int>& indices);

  // TabGroupController:
  void CreateTabGroup(const tab_groups::TabGroupId& group) override;
  void OpenTabGroupEditor(const tab_groups::TabGroupId& group) override;
  void ChangeTabGroupContents(const tab_groups::TabGroupId& group) override;
  void ChangeTabGroupVisuals(
      const tab_groups::TabGroupId& group,
      const TabGroupChange::VisualsChange& visuals) override;
  void MoveTabGroup(const tab_groups::TabGroupId& group) override;
  void CloseTabGroup(const tab_groups::TabGroupId& group) override;
  std::u16string GetTitleAt(int index) const override;
  // The same as count(), but overridden for TabGroup to access.
  int GetTabCount() const override;

  // View API //////////////////////////////////////////////////////////////////

  // Context menu functions. Tab groups uses command ids following CommandLast
  // for entries in the 'Add to existing group' submenu.
  enum ContextMenuCommand {
    CommandFirst,
    CommandNewTabToRight,
    CommandReload,
    CommandDuplicate,
    CommandCloseTab,
    CommandCloseOtherTabs,
    CommandCloseTabsToRight,
    CommandTogglePinned,
    CommandToggleGrouped,
    CommandToggleSiteMuted,
    CommandSendTabToSelf,
    CommandAddNote,
    CommandAddToReadLater,
    CommandAddToNewGroup,
    CommandAddToExistingGroup,
    CommandRemoveFromGroup,
    CommandMoveToExistingWindow,
    CommandMoveTabsToNewWindow,
    CommandOrganizeTabs,
    CommandCopyURL,
    CommandGoBack,
    CommandCloseAllTabs,
    CommandCommerceProductSpecifications,
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

  // Returns a list of the group ids that are going to be deleted if a given
  // list of tab indexes are removed from the group. used by context menu
  // commands to decide whether to confirm group deletion.
  std::vector<tab_groups::TabGroupId> GetGroupsDestroyedFromRemovingIndices(
      const std::vector<int>& indices) const;

  // There are multiple commands that close by indices. They all must check the
  // Group affiliation of the indices, confirm that they can delete groups, and
  // then perform the close of the indices. A bulk operation is denoted
  // as an operation that closes multiple tabs while leaving one or more
  // tabs open in this window.
  void ExecuteCloseTabsByIndicesCommand(
      base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
      bool is_bulk_operation);

  // Adds the tab at |context_index| to the given tab group |group|. If
  // |context_index| is selected the command applies to all selected tabs.
  void ExecuteAddToExistingGroupCommand(int context_index,
                                        const tab_groups::TabGroupId& group);

  // Adds the tab at |context_index| to the browser window at |browser_index|.
  // If |context_index| is selected the command applies to all selected tabs.
  void ExecuteAddToExistingWindowCommand(int context_index, int browser_index);

  // Returns true if 'CommandToggleSiteMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuMuteSites(int index);

  // Returns true if 'CommandTogglePinned' will pin. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuPin(int index);

  // Returns true if 'CommandToggleGrouped' will group. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuGroup(int index);

  // Convert a ContextMenuCommand into a browser command. Returns true if a
  // corresponding browser command exists, false otherwise.
  static bool ContextMenuCommandToBrowserCommand(int cmd_id, int* browser_cmd);

  // Returns the index of the next WebContents in the sequence of WebContentses
  // spawned by the specified WebContents after |start_index|.
  int GetIndexOfNextWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index) const;

  // Finds the next available tab to switch to as the active tab starting at
  // |index|. This method will check the indices to the right of |index| before
  // checking the indices to the left of |index|. |index| cannot be returned.
  // |collapsing_group| is optional and used in cases where the group is
  // collapsing but not yet reflected in the model. Returns std::nullopt if
  // there are no valid tabs.
  std::optional<int> GetNextExpandedActiveTab(
      int index,
      std::optional<tab_groups::TabGroupId> collapsing_group) const;

  // Forget all opener relationships, to reduce unpredictable tab switching
  // behavior in complex session states.
  void ForgetAllOpeners();

  // Forgets the opener relationship of the specified WebContents.
  void ForgetOpener(content::WebContents* contents);

  // Determine where to place a newly opened tab by using the supplied
  // transition and foreground flag to figure out how it was opened.
  int DetermineInsertionIndex(ui::PageTransition transition, bool foreground);

  // If a tab is in a group and the tab failed to close, this method will be
  // called from the unload_controller. Ungroup the group to maintain
  // consistency with the user's intended action (to get rid of the group).
  void GroupCloseStopped(const tab_groups::TabGroupId& group);

  // Serialise this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Convert between tabs and indices.
  int GetIndexOfTab(const tabs::TabModel* tab) const;
  tabs::TabModel* GetTabAtIndex(int index) const;

  // TODO(349161508) remove this method once tabs dont need to be converted
  // into webcontents.
  tabs::TabModel* GetTabForWebContents(
      const content::WebContents* contents) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, GetIndicesClosedByCommand);

  struct DetachNotifications;
  struct MoveNotification {
    int initial_index;
    std::optional<tab_groups::TabGroupId> intial_group;
    tabs::TabHandle handle;
    TabStripSelectionChange selection_change;
  };

  // Tracks whether a tabstrip-modal UI is showing.
  class ScopedTabStripModalUIImpl : public ScopedTabStripModalUI {
   public:
    explicit ScopedTabStripModalUIImpl(TabStripModel* model);
    ~ScopedTabStripModalUIImpl() override;

   private:
    // Owns this.
    raw_ptr<TabStripModel> model_;
  };

  // Perform tasks associated with changes to the model. Change the Active Index
  // and notify observers.
  void OnChange(const TabStripModelChange& change,
                const TabStripSelectionChange& selection);

  // Detaches the WebContents at the specified |index| from this strip. |reason|
  // is used to indicate to observers what is going to happen to the WebContents
  // (i.e. deleted or reinserted into another tab strip). Returns the detached
  // WebContents.
  std::unique_ptr<DetachedWebContents> DetachWebContentsWithReasonAt(
      int index,
      TabStripModelChange::RemoveReason reason);

  // Performs all the work to detach a WebContents instance but avoids sending
  // most notifications. TabClosingAt() and TabDetachedAt() are sent because
  // observers are reliant on the selection model being accurate at the time
  // that TabDetachedAt() is called.
  std::unique_ptr<DetachedWebContents> DetachWebContentsImpl(
      int index_before_any_removals,
      int index_at_time_of_removal,
      bool create_historical_tab,
      TabStripModelChange::RemoveReason reason);

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

  int ConstrainInsertionIndex(int index, bool pinned_tab) const;

  int ConstrainMoveIndex(int index, bool pinned_tab) const;

  // If |index| is selected all the selected indices are returned, otherwise a
  // vector with |index| is returned. This is used when executing commands to
  // determine which indices the command applies to. Indices are sorted in
  // increasing order.
  std::vector<int> GetIndicesForCommand(int index) const;

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  std::vector<int> GetIndicesClosedByCommand(int index,
                                             ContextMenuCommand id) const;

  // Returns true if the specified WebContents is a New Tab at the end of
  // the tabstrip. We check for this because opener relationships are _not_
  // forgotten for the New Tab page opened as a result of a New Tab gesture
  // (e.g. Ctrl+T, etc) since the user may open a tab transiently to look up
  // something related to their current activity.
  bool IsNewTabAtEndOfTabStrip(content::WebContents* contents) const;

  // Adds the specified TabModel at the specified location.
  // |add_types| is a bitmask of AddTabTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddTabTypes is meaningless here. The only time
  // the |index| is changed is if using the index would result in breaking the
  // constraint that all pinned tabs occur before non-pinned tabs. It returns
  // the index the tab is actually inserted to. See also AddWebContents.
  int InsertTabAtImpl(int index,
                      std::unique_ptr<tabs::TabModel> tab,
                      int add_types,
                      std::optional<tab_groups::TabGroupId> group);

  // Closes the WebContentses at the specified indices. This causes the
  // WebContentses to be destroyed, but it may not happen immediately. If
  // the page in question has an unload event the WebContents will not be
  // destroyed until after the event has completed, which will then call back
  // into this method.
  void CloseTabs(base::span<content::WebContents* const> items,
                 uint32_t close_types);

  // Executes a call to CloseTabs on the web contentses contained in tabs
  // returned from |get_indices_to_close|. This is a helper method
  // bound by ExecuteCloseTabsByIndicesCommand in order to properly
  // protect the stack from reentrancy.
  void ExecuteCloseTabsByIndices(
      base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
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
                          uint32_t close_types,
                          DetachNotifications* notifications);

  // Returns the WebContentses at the specified indices. This does no checking
  // of the indices, it is assumed they are valid.
  std::vector<content::WebContents*> GetWebContentsesByIndices(
      const std::vector<int>& indices) const;

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

  // direction of relative tab movements or selections. kNext indicates moving
  // forward (positive increment) in the tab strip. kPrevious indicates
  // backward (negative increment).
  enum class TabRelativeDirection {
    kNext,
    kPrevious,
  };

  // Selects either the next tab (kNext), or the previous tab (kPrevious).
  void SelectRelativeTab(TabRelativeDirection direction,
                         TabStripUserGestureDetails detail);

  // Moves the active tabs into the next slot (kNext), or the
  // previous slot (kPrevious). Respects group boundaries and creates
  // movement slots into and out of groups.
  void MoveTabRelative(TabRelativeDirection direction);

  // Implementation of MoveSelectedTabsTo. Moves |length| of the selected tabs
  // starting at |start| to |index|. See MoveSelectedTabsTo for more details.
  void MoveSelectedTabsToImpl(int index, size_t start, size_t length);

  std::vector<int> GetSelectedPinnedTabs();
  std::vector<int> GetSelectedUnpinnedTabs();

  // Adds tabs to newly-allocated group id |new_group|. This group must be new
  // and have no tabs in it.
  void AddToNewGroupImpl(
      const std::vector<int>& indices,
      const tab_groups::TabGroupId& new_group,
      std::optional<tab_groups::TabGroupVisualData> visual_data = std::nullopt);

  void MoveGroupToImpl(const tab_groups::TabGroupId& group, int to_index);

  // Adds tabs to existing group |group|. This group must have been initialized
  // by a previous call to |AddToNewGroupImpl()|.
  void AddToExistingGroupImpl(const std::vector<int>& indices,
                              const tab_groups::TabGroupId& group,
                              const bool add_to_end = false);

  // Implementation of MoveTabsAndSetGroupImpl. Moves the set of tabs in
  // |indices| to the |destination_index| and updates the tabs to the
  // appropriate |group|.
  void MoveTabsAndSetGroupImpl(const std::vector<int>& indices,
                               int destination_index,
                               std::optional<tab_groups::TabGroupId> group);

  void AddToReadLaterImpl(const std::vector<int>& indices);

  // Updates the `contents_data_` and sends out observer notifications for
  // inserting a new tab in  the tabstrip.
  void InsertTabAtIndexImpl(std::unique_ptr<tabs::TabModel> tab_model,
                            int index,
                            std::optional<tab_groups::TabGroupId> group,
                            bool pin,
                            bool active);

  // Updates the `contents_data_` and sends out observer notifications for
  // removing an existing tab in  the tabstrip.
  std::unique_ptr<tabs::TabModel> RemoveTabFromIndexImpl(int index);

  // Updates the `contents_data_` and sends out observer notifications for
  // updating the index, pinned state or group property.
  void MoveTabToIndexImpl(int initial_index,
                          int final_index,
                          const std::optional<tab_groups::TabGroupId> group,
                          bool pin,
                          bool select_after_move);

  // Similar to `MoveTabToIndexImpl` but this is used for multiple tabs either
  // being moved or having their group updated. `tab_indices` should be sorted.
  void MoveTabsToIndexImpl(const std::vector<int>& tab_indices,
                           int destination_index,
                           const std::optional<tab_groups::TabGroupId> group);

  // Sends group notifications for a tab at `index` based on its initial_group
  // and `final_group` and updates the `group_model_`.
  void TabGroupStateChanged(
      int index,
      tabs::TabModel* tab,
      const std::optional<tab_groups::TabGroupId> initial_group,
      const std::optional<tab_groups::TabGroupId> new_group);

  // Updates the `group_model` by decrementing the tab count of `group`.
  void RemoveTabFromGroupModel(const tab_groups::TabGroupId& group);

  // Updates the `group_model` by incrementing the tab count of `group`.
  void AddTabToGroupModel(const tab_groups::TabGroupId& group);

  // Checks if the `contents_data_` is in a valid order. This checks for
  // pinned tabs placement, group contiguity and selected tabs validity.
  void ValidateTabStripModel();

  void SendMoveNotificationForWebContents(
      int index,
      int to_position,
      content::WebContents* web_contents,
      TabStripSelectionChange& selection_change);

  TabStripSelectionChange MaybeUpdateSelectionModel(int initial_index,
                                                    int final_index,
                                                    bool select_after_move);

  // Generates the MoveNotifications for `MoveTabsToIndexImpl` and updates the
  // selection model and openers.
  std::vector<TabStripModel::MoveNotification> PrepareTabsToMoveToIndex(
      const std::vector<int>& tab_indices,
      int destination_index);

  // Generates a sequence of initial and destination index for tabs in
  // `tab_indices` when the tabs need to move to `destination_index`.
  std::vector<std::pair<int, int>> CalculateIncrementalTabMoves(
      const std::vector<int>& tab_indices,
      int destination_index) const;

  // Changes the pinned state of all tabs at `indices`, moving them in the
  // process if necessary.
  void SetTabsPinned(const std::vector<int> indices, bool pinned);

  // Sets the sound content setting for each site at the |indices|.
  void SetSitesMuted(const std::vector<int>& indices, bool mute) const;

  // Sets the opener of any tabs that reference the tab at |index| to that tab's
  // opener or null if there's a cycle.
  void FixOpeners(int index);

  // Returns a group when the index of a tab is updated from `index` to
  // `to_position` that would not break group contiguity. The group returned
  // keeps the original group if it is valid at the `to_position` and otherwise
  // returns a valid group.
  std::optional<tab_groups::TabGroupId> GetGroupToAssign(int index,
                                                         int to_position);

  // Returns a valid index to be selected after the tab at |removing_index| is
  // closed. If |index| is after |removing_index|, |index| is adjusted to
  // reflect the fact that |removing_index| is going away.
  int GetTabIndexAfterClosing(int index, int removing_index) const;

  // Takes the |selection| change and decides whether to forget the openers.
  void OnActiveTabChanged(const TabStripSelectionChange& selection);

  // Checks if policy allows a tab to be closed.
  bool PolicyAllowsTabClosing(content::WebContents* contents) const;

  // Determine where to shift selection after a tab is closed.
  std::optional<int> DetermineNewSelectedIndex(int removed_index) const;

  // The WebContents data currently hosted within this TabStripModel. This must
  // be kept in sync with |selection_model_|.
  std::unique_ptr<TabContentsData> contents_data_;

  // The model for tab groups hosted within this TabStripModel.
  std::unique_ptr<TabGroupModel> group_model_;

  raw_ptr<TabStripModelDelegate, DanglingUntriaged> delegate_;

  bool tab_strip_ui_was_set_ = false;

  base::ObserverList<TabStripModelObserver>::UncheckedAndDanglingUntriaged
      observers_;

  // A profile associated with this TabStripModel.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_ = false;

  // This must be kept in sync with |contents_data_|.
  ui::ListSelectionModel selection_model_;

  // TabStripModel is not re-entrancy safe. This member is used to guard public
  // methods that mutate state of |selection_model_| or |contents_data_|.
  bool reentrancy_guard_ = false;

  TabStripScrubbingMetrics scrubbing_metrics_;

  // Tracks whether a modal UI is showing.
  bool showing_modal_ui_ = false;

  base::WeakPtrFactory<TabStripModel> weak_factory_{this};
};

// Forbid construction of ScopedObservation and ScopedMultiSourceObservation
// with TabStripModel: TabStripModelObserver already implements their
// functionality natively.
namespace base {

template <>
class ScopedObservation<TabStripModel, TabStripModelObserver> {
 public:
  // Deleting the constructor gives a clear error message traceable back to here.
  explicit ScopedObservation(TabStripModelObserver* observer) = delete;
};

template <>
class ScopedMultiSourceObservation<TabStripModel, TabStripModelObserver> {
 public:
  // Deleting the constructor gives a clear error message traceable back to
  // here.
  explicit ScopedMultiSourceObservation(TabStripModelObserver* observer) =
      delete;
};

}  // namespace base

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
