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
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_scrubbing_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class DraggingTabsSession;
class Profile;
class TabGroupModel;
class TabStripModelDelegate;
class TabStripModelObserver;
class TabStripServiceImpl;

namespace content {
class WebContents;
}

namespace split_tabs {
class SplitTabData;
class SplitTabVisualData;
enum class SplitTabLayout;
}

namespace tabs {
class SplitTabCollection;
class TabStripCollection;
class TabGroupTabCollection;
}

class TabGroupModelFactory {
 public:
  TabGroupModelFactory();
  TabGroupModelFactory(const TabGroupModelFactory&) = delete;
  TabGroupModelFactory& operator=(const TabGroupModelFactory&) = delete;

  static TabGroupModelFactory* GetInstance();
  std::unique_ptr<TabGroupModel> Create();
};

// Have DetachedTabCollection object as a container of the `collection_` so
// client does not need to worry or deal with the collection object.
struct DetachedTabCollection {
  DetachedTabCollection(
      std::variant<std::unique_ptr<tabs::TabGroupTabCollection>,
                   std::unique_ptr<tabs::SplitTabCollection>> collection,
      std::optional<int> active_index,
      bool pinned_);
  DetachedTabCollection(const DetachedTabCollection&) = delete;
  DetachedTabCollection& operator=(const DetachedTabCollection&) = delete;
  ~DetachedTabCollection();
  DetachedTabCollection(DetachedTabCollection&&);
  std::variant<std::unique_ptr<tabs::TabGroupTabCollection>,
               std::unique_ptr<tabs::SplitTabCollection>>
      collection_;
  // Store the index of tab that was active in the detached group.
  std::optional<int> active_index_ = std::nullopt;
  bool pinned_ = false;
};

// Holds state for a tab that has been detached from the tab strip.
// Will also handle tab deletion if `remove_reason` is kDeleted.
struct DetachedTab {
  DetachedTab(int index_before_any_removals,
              int index_at_time_of_removal,
              std::unique_ptr<tabs::TabModel> tab,
              TabStripModelChange::RemoveReason remove_reason,
              tabs::TabInterface::DetachReason tab_detach_reason,
              std::optional<SessionID> id);
  DetachedTab(const DetachedTab&) = delete;
  DetachedTab& operator=(const DetachedTab&) = delete;
  ~DetachedTab();
  DetachedTab(DetachedTab&&);

  std::unique_ptr<tabs::TabModel> tab;

  // The index of the tab in the original selection model of the tab
  // strip [prior to any tabs being removed, if multiple tabs are being
  // simultaneously removed].
  const int index_before_any_removals;

  // The index of the tab at the time it is being removed. If multiple
  // tabs are being simultaneously removed, the index reflects previously
  // removed tabs in this batch.
  const int index_at_time_of_removal;

  // Reasons for detaching a tab. These may differ, for e.g. when a
  // tab is detached for re-insertion into a browser of different type,
  // in which case the TabInterface is destroyed but the WebContents is
  // retained.
  TabStripModelChange::RemoveReason remove_reason;
  tabs::TabInterface::DetachReason tab_detach_reason;

  // The |contents| associated optional SessionID, used as key for
  // ClosedTabCache. We only cache |contents| if |remove_reason| is kCached.
  //
  // TODO(crbug.com/377537302): The ClosedTabCache feature is gone, but it's
  // unclear if the session ID is needed for other things as well.
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
class TabStripModel {
 public:
  using TabIterator = tabs::TabCollection::TabIterator;
  using CollectionIterator = tabs::TabCollection::Iterator;

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

  ~TabStripModel();

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

  // TODO(crbug.com/417291958) remove this function since its the same as
  // count().
  int GetTabCount() const;

  bool empty() const;

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

  // Creates a group object so that group_model can link it with once group
  // collection owns it.
  // TODO(crbug.com/392952244): Remove this after replacing callers with
  // detaching and attaching groups.
  void AddTabGroup(const tab_groups::TabGroupId group_id,
                   tab_groups::TabGroupVisualData visual_data);

  // Adds a TabModel from another tabstrip at the specified location. See
  // InsertWebContentsAt.
  int InsertDetachedTabAt(
      int index,
      std::unique_ptr<tabs::TabModel> tab,
      int add_types,
      std::optional<tab_groups::TabGroupId> group = std::nullopt);

  // Removes the group collection from the collection hierarchy and passes it to
  // the client. The client can re-insert into another tabstrip using
  // `InsertDetachedGroupAt` without destroying the group.
  std::unique_ptr<DetachedTabCollection> DetachTabGroupForInsertion(
      const tab_groups::TabGroupId group_id);

  // Inserts a detached tab group into the tabstrip starting at `index`.
  gfx::Range InsertDetachedTabGroupAt(
      std::unique_ptr<DetachedTabCollection> group,
      int index);

  // Removes the split collection from the collection hierarchy and passes it to
  // the client. The client can re-insert into another tabstrip using
  // `InsertDetachedSplitTabAt` without destroying the split.
  std::unique_ptr<DetachedTabCollection> DetachSplitTabForInsertion(
      const split_tabs::SplitTabId split_id);

  // Inserts a detached split tab into the tabstrip starting at `index`.
  // `pinned` and `group` information are used to insert it in the right place
  // in the collection hierarchy.
  gfx::Range InsertDetachedSplitTabAt(
      std::unique_ptr<DetachedTabCollection> split,
      int index,
      bool pinned,
      std::optional<tab_groups::TabGroupId> group_id = std::nullopt);

  // Closes the WebContents at the specified index. This causes the
  // WebContents to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes.
  // TODO(crbug.com/392950857): Currently many call sites of CloseWebContentsAt
  // convert a tab/webcontents to an index, which gets converted back to a
  // webcontents within this function. Provide a CloseWebContents function that
  // directly closes a web contents so that we don't have to convert back and
  // forth.
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

  // Detaches the WebContents at the specified index for re-insertion into
  // another browser of a different type, destroying the owning TabModel in the
  // process.
  //
  // This works as follows:
  //   - the contents is extracted from the source browser and the owning tab is
  //     destroyed (performed by DetachWebContentsAtForInsertion())
  //   - the contents is added to the new browser, creating a new tab model
  //
  // TODO(crbug.com/334281979): This is done to avoid TabFeatures having to deal
  // with changing browser types during tab moves. This should no longer be
  // necessary once non-normal browser windows do not use Browser, TabStripModel
  // or TabModel.
  std::unique_ptr<content::WebContents> DetachWebContentsAtForInsertion(
      int index);

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

  // Moves all tabs in `group` to `to_index`. This has no checks to make sure
  // the position is valid for a group to move to.
  void MoveGroupTo(const tab_groups::TabGroupId& group, int to_index);

  // Moves all tabs in split with `split_id` to `to_index` with  properties
  // `pinned` and `group_id`. This has no checks to make sure the position is
  // valid for a split to move to.
  void MoveSplitTo(const split_tabs::SplitTabId& split_id,
                   int to_index,
                   bool pinned,
                   std::optional<tab_groups::TabGroupId> group_id);

  // Returns the currently active WebContents, or NULL if there is none.
  content::WebContents* GetActiveWebContents() const;

  // Returns the currently active Tab, or NULL if there is none.
  tabs::TabInterface* GetActiveTab() const;

  // Returns the WebContents at the specified index, or NULL if there is
  // none.
  content::WebContents* GetWebContentsAt(int index) const;

  // Returns the index of the specified WebContents, or TabStripModel::kNoTab
  // if the WebContents is not in this TabStripModel.
  int GetIndexOfWebContents(const content::WebContents* contents) const;

  // Notify any observers that the tab has changed in some way. See
  // TabChangeType for details of |change_type|.'
  void NotifyTabChanged(const tabs::TabInterface* const tab,
                        TabChangeType change_type);

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

  // Returns true if there are any WebContentses that are currently loading
  // and should be shown on the UI.
  bool TabsNeedLoadingUI() const;

  // Returns the WebContents that opened the WebContents at |index|, or NULL if
  // there is no opener on record.
  tabs::TabInterface* GetOpenerOfTabAt(const int index) const;

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

  split_tabs::SplitTabData* GetSplitData(split_tabs::SplitTabId split_id) const;

  bool ContainsSplit(split_tabs::SplitTabId split_id) const;

  std::optional<split_tabs::SplitTabId> GetSplitForTab(int index) const;

  // Returns the group that contains the tab at |index|, or nullopt if the tab
  // index is invalid or not grouped.
  std::optional<tab_groups::TabGroupId> GetTabGroupForTab(int index) const;

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

  // This can fail if the tabstrip is not editable.
  void SelectTabAt(int index);

  // This can fail if the tabstrip is not editable.
  void DeselectTabAt(int index);

  // Makes sure the tabs from the anchor to |index| are selected. This adds to
  // the selection if there is an anchor and resets the selection to |index| if
  // there is not an anchor.
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
  GetAdjacentTabsAfterSelectedMove(base::PassKey<DraggingTabsSession>,
                                   int destination_index);

  // Updates the layout for the tabs with `split_id` and notifies observers.
  void UpdateSplitLayout(split_tabs::SplitTabId split_id,
                         split_tabs::SplitTabLayout tab_layout);

  // Updates the ratio for the tabs with `split_id` and notifies observers.
  void UpdateSplitRatio(split_tabs::SplitTabId split_id,
                        double start_content_ratio);

  // Updates the active tab within `split_id` with the tab at `update_index`.
  enum class SplitUpdateType { kReplace, kSwap };
  void UpdateActiveTabInSplit(split_tabs::SplitTabId split_id,
                              int update_index,
                              SplitUpdateType update_type);

  // Reverses the order of tabs with `split_id`.
  void ReverseTabsInSplit(split_tabs::SplitTabId split_id);

  // Create a new split view with the active tab and add the set of tabs pointed
  // to by |indices| to it. Reorders the tabs so they are contiguous. |indices|
  // must be sorted in ascending order.
  split_tabs::SplitTabId AddToNewSplit(
      const std::vector<int> indices,
      split_tabs::SplitTabVisualData visual_data);

  // Create a new tab group and add the set of tabs pointed to be |indices| to
  // it. Pins all of the tabs if any of them were pinned, and reorders the tabs
  // so they are contiguous and do not split an existing group in half. Returns
  // the new group. |indices| must be sorted in ascending order.
  tab_groups::TabGroupId AddToNewGroup(const std::vector<int> indices);

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

  // Unsplits all the tabs that are part of the split with `split_id`. The tabs
  // maintain their group and pin properties.
  void RemoveSplit(split_tabs::SplitTabId split_id);

  TabGroupModel* group_model() const { return group_model_.get(); }

  bool SupportsTabGroups() const { return group_model_.get() != nullptr; }

  // Returns true if one or more of the tabs pointed to by |indices| are
  // supported by read later.
  bool IsReadLaterSupportedForAny(const std::vector<int>& indices);

  // Saves tabs with url supported by Read Later.
  void AddToReadLater(const std::vector<int>& indices);

  // Notifies all group observers that the TabGroupEditor is opening. This is
  // used by Views that want to force the editor to open without having to find
  // the group's header view in the Tab Strip.
  void OpenTabGroupEditor(const tab_groups::TabGroupId& group);

  // Updates the group visuals and notifies observers.
  void ChangeTabGroupVisuals(const tab_groups::TabGroupId& group,
                             tab_groups::TabGroupVisualData visual_data,
                             bool is_customized = false);

  // Returns iterators for traversing through all the tabs in the tabstrip.
  TabIterator begin() const;
  TabIterator end() const;

  CollectionIterator collection_begin(
      base::PassKey<TabStripServiceImpl> key) const;
  CollectionIterator collection_end(
      base::PassKey<TabStripServiceImpl> key) const;

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
    CommandAddToNewGroupFromMenuItem,
    CommandAddToNewComparisonTable,
    CommandAddToExistingComparisonTable,
    CommandAddToSplit,
    CommandSwapWithActiveSplit,
    CommandArrangeSplit,
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

  // This should be called after GetGroupsDestroyedFromRemovingIndices(). Marks
  // all groups in `group_ids` as closing. This is useful in the event you need
  // to know if a group is currently closing or not such as when a grouped tab
  // is closed which has an unload handler.
  void MarkTabGroupsForClosing(
      const std::vector<tab_groups::TabGroupId> group_ids);

  // There are multiple commands that close by indices. They all must check the
  // Group affiliation of the indices, confirm that they can delete groups, and
  // then perform the close of the indices. When true `delete_groups` also
  // deletes any saved groups that are closing. When false, groups will close
  // normally but continue to be saved.
  void ExecuteCloseTabsByIndicesCommand(
      base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
      bool delete_groups);

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

  // Returns the index of the next tab spawned by the specified tabs in
  // `block_tab_range`.
  int GetIndexOfNextWebContentsOpenedBy(
      const gfx::Range& block_tab_range) const;

  // Returns the index of the next tab spawned by the opener of the specified
  // tabs in `block_tab_range`.
  int GetIndexOfNextWebContentsOpenedByOpenerOf(
      const gfx::Range& block_tab_range) const;

  // Finds the next available tab to switch to as the active tab starting at
  // a block of tabs. The methods will check the indices to
  // the right of the block before checking the indices to the left of the
  // block. Index within the block cannot be returned. Returns std::nullopt if
  // there are no valid tabs.
  std::optional<int> GetNextExpandedActiveTab(
      const gfx::Range& block_tab_range) const;
  std::optional<int> GetNextExpandedActiveTab(
      tab_groups::TabGroupId collapsing_group) const;

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
  int GetIndexOfTab(const tabs::TabInterface* tab) const;
  tabs::TabInterface* GetTabAtIndex(int index) const;

  // TODO(349161508) remove this method once tabs dont need to be converted
  // into webcontents.
  tabs::TabInterface* GetTabForWebContents(
      const content::WebContents* contents) const;

  // Returns [start, end) where the leftmost tab in the split has index start
  // and the rightmost tab in the split has index end - 1.
  gfx::Range GetIndexRangeOfSplit(split_tabs::SplitTabId split_id) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabStripModelTest, GetIndicesClosedByCommand);

  struct DetachNotifications;
  struct MoveNotification {
    int initial_index;
    std::optional<tab_groups::TabGroupId> intial_group;
    bool initial_pinned;
    raw_ptr<const tabs::TabInterface> tab;
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

  tabs::TabModel* GetTabModelAtIndex(int index) const;

  // Perform tasks associated with changes to the model. Change the Active Index
  // and notify observers.
  void OnChange(const TabStripModelChange& change,
                const TabStripSelectionChange& selection);

  // Notify observers that a `group` was created.
  void NotifyTabGroupVisualsChanged(const tab_groups::TabGroupId& group_id,
                                    TabGroupChange::VisualsChange visuals);

  // Notify observers that a `group` was created.
  void NotifyTabGroupCreated(const tab_groups::TabGroupId& group);

  // Notify observers that a `group` was closed.
  void NotifyTabGroupClosed(const tab_groups::TabGroupId& group);

  // Notify observers that `group` is moved.
  void NotifyTabGroupMoved(const tab_groups::TabGroupId& group);

  // Notify observers that `group` is detached from the model. This also sends
  // split related observations within the group.
  void NotifyTabGroupDetached(
      tabs::TabGroupTabCollection* group_collection,
      std::map<split_tabs::SplitTabId,
               std::vector<std::pair<tabs::TabInterface*, int>>>
          splits_in_group);

  // Notify observers that `group` is attached to the model. This also sends
  // split related observations within the group.
  void NotifyTabGroupAttached(tabs::TabGroupTabCollection* group_collection);

  // Notify observers that split with `split_id` has been created.
  void NotifySplitTabCreated(
      split_tabs::SplitTabId split_id,
      const std::vector<std::pair<tabs::TabInterface*, int>>& tabs_with_indices,
      SplitTabChange::SplitTabAddReason reason,
      const split_tabs::SplitTabVisualData& visual_data);

  // Notify observers that visual data for a split has changed.
  void NotifySplitTabVisualsChanged(
      split_tabs::SplitTabId split_id,
      const split_tabs::SplitTabVisualData& old_visual_data,
      const split_tabs::SplitTabVisualData& new_visual_data);

  // Notify observers that contents of a split has been reordered.
  void NotifySplitTabContentsUpdated(
      split_tabs::SplitTabId split_id,
      const std::vector<std::pair<tabs::TabInterface*, int>>& prev_tabs,
      const std::vector<std::pair<tabs::TabInterface*, int>>& new_tabs);

  // Notify observers that split with `split_id` has been removed.
  void NotifySplitTabRemoved(
      split_tabs::SplitTabId split_id,
      const std::vector<std::pair<tabs::TabInterface*, int>>& tabs_with_indices,
      SplitTabChange::SplitTabRemoveReason reason);

  // Notify observers that a split was detached from this tabstrip model.
  // This also sends any group related notification.
  void NotifySplitTabDetached(
      tabs::SplitTabCollection* split_collection,
      std::vector<std::pair<tabs::TabInterface*, int>> tabs_in_split,
      std::optional<tab_groups::TabGroupId> previous_group_state);

  // Notify observers that a split was attached to this tabstrip model.
  // This also sends any group related notification.
  void NotifySplitTabAttached(tabs::SplitTabCollection* split_collection);

  // Detaches the tab at the specified `index` from this strip.
  // `web_contents_remove_reason` is used to indicate to observers what is going
  // to happen to the WebContents (i.e. deleted or reinserted into another tab
  // strip). `tab_detach_reason` is used to indicate to observers what is going
  // to happen to the TabModel owning the WebContents. These reasons may not
  // always match (a WebContents may be retained for re-insertion while its
  // owning TabModel may be destroyed).
  std::unique_ptr<DetachedTab> DetachTabWithReasonAt(
      int index,
      TabStripModelChange::RemoveReason web_contents_remove_reason,
      tabs::TabInterface::DetachReason tab_detach_reason);

  // Performs all the work to detach a TabModel instance but avoids sending
  // most notifications. TabClosingAt() and TabDetachedAt() are sent because
  // observers are reliant on the selection model being accurate at the time
  // that TabDetachedAt() is called.
  std::unique_ptr<DetachedTab> DetachTabImpl(
      int index_before_any_removals,
      int index_at_time_of_removal,
      bool create_historical_tab,
      TabStripModelChange::RemoveReason web_contents_remove_reason,
      tabs::TabInterface::DetachReason tab_detach_reason);

  // Removes a tab collection from `contents_data_` using
  // `execute_detach_collection_operation`. Also sends collection specific
  // observation using `execute_tabs_notify_observer_operation` like group and
  // split related observation calls. `TabStripModelChange` and
  // `TabStripSelectionChange` observation calls are handled as common code.
  std::unique_ptr<tabs::TabCollection> DetachTabCollectionImpl(
      tabs::TabCollection* collection,
      base::OnceCallback<std::unique_ptr<tabs::TabCollection>()>
          execute_detach_collection_operation,
      base::OnceClosure execute_tabs_notify_observer_operation);

  // Helper method performing tasks like notification, fixing opener and
  // returning back a Remove struct before actually detaching the set of
  // tab_indices.
  TabStripModelChange::Remove ProcessTabsForDetach(gfx::Range tab_indices);

  // Helper method for updating the selection model after detaching a collection
  // from `contents_data_`.
  void UpdateSelectionModelForDetach(gfx::Range tab_indices,
                                     std::optional<int> next_selected_index);

  // Attaches a tab collection to `contents_data_` using
  // `execute_insert_detached_tabs_operation`. Also sends collection specific
  // observation using `execute_tabs_notify_observer_operation` like group and
  // split related observation calls. `TabStripModelChange` and
  // `TabStripSelectionChange` observation calls are handled as common code.
  gfx::Range InsertDetachedCollectionImpl(
      tabs::TabCollection* collection,
      std::optional<int> active_index,
      base::OnceClosure execute_insert_detached_tabs_operation,
      base::OnceClosure execute_tabs_notify_observer_operation);

  // This is the callback used as `execute_insert_detached_tabs_operation` in
  // `InsertDetachedCollectionImpl` when a group is inserted into a tabstrip. It
  // updates the `group_model_` and inserts the `group_collection` into
  // `contents_data_`.
  void InsertDetachedTabGroupImpl(
      std::unique_ptr<tabs::TabGroupTabCollection> group_collection,
      int index);

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

  // Direction of relative tab movements or selections. kNext indicates moving
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

  split_tabs::SplitTabId AddToSplitImpl(
      split_tabs::SplitTabId split_id,
      std::vector<int> indices,
      split_tabs::SplitTabVisualData visual_data,
      SplitTabChange::SplitTabAddReason reasons);

  void RemoveSplitImpl(split_tabs::SplitTabId split_id,
                       SplitTabChange::SplitTabRemoveReason reason);

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

  // Adds all selected indices provided by `context_index` into a new tab group.
  void AddToNewGroupFromContextIndex(int context_index);

  // Implementation of MoveTabsAndSetPropertiesImpl. Moves the set of tabs in
  // |indices| to the |destination_index| and updates the tabs to the
  // appropriate |group| and |pinned| properties.
  void MoveTabsAndSetPropertiesImpl(const std::vector<int>& indices,
                                    int destination_index,
                                    std::optional<tab_groups::TabGroupId> group,
                                    bool pinned);

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
  std::unique_ptr<tabs::TabModel> RemoveTabFromIndexImpl(
      int index,
      tabs::TabInterface::DetachReason tab_detach_reason);

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
      tabs::TabInterface* tab,
      const std::optional<tab_groups::TabGroupId> initial_group,
      const std::optional<tab_groups::TabGroupId> new_group);

  // Updates the `group_model` by decrementing the tab count of `group`.
  void RemoveTabFromGroupModel(const tab_groups::TabGroupId& group);

  // Updates the `group_model` by incrementing the tab count of `group`.
  void AddTabToGroupModel(const tab_groups::TabGroupId& group);

  // Checks if the `contents_data_` is in a valid order. This checks for
  // pinned tabs placement, group contiguity and selected tabs validity.
  void ValidateTabStripModel();

  void SendMoveNotificationForTab(
      int index,
      int to_position,
      tabs::TabInterface* tab,
      const TabStripSelectionChange& selection_change);

  void UpdateSelectionModelForMove(int initial_index,
                                   int final_index,
                                   bool select_after_move);

  void UpdateSelectionModelForMoves(const std::vector<int>& tab_indices,
                                    int destination_index);

  // Clears any previous selection and sets the selected index. This takes into
  // account split tabs so both will be selected if `index` is a split tab.
  void SetSelectedIndex(ui::ListSelectionModel* selection, int index);

  // Returns the range of indices between the anchor and a provided index, that
  // takes into account split tabs. If the anchor or the tab at index is part of
  // a split, the range will include that split. The start and end indices are
  // inclusive.
  std::pair<int, int> GetSelectionRangeFromAnchorToIndex(int index);

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
  // process if necessary. If indices contains all tabs in a split, the whole
  // split is pinned/unpinned. Otherwise, the tabs will be individually
  // processed, resulting in the split being unsplit.
  void SetTabsPinned(const std::vector<int> indices, bool pinned);

  // Implementation for setting the pinned state of the tab at `index`.
  int SetTabPinnedImpl(int indices, bool pinned);

  // Changes the pinned state of a split collection, moving it in the process if
  // necessary.
  void SetSplitPinnedImpl(tabs::SplitTabCollection* split, bool pinned);

  // Wrapper for bulk move operations to make them send out the appropriate
  // change notifications.
  void MoveTabsWithNotifications(std::vector<int> tab_indices,
                                 int destination_index,
                                 base::OnceClosure execute_tabs_move_operation);

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

  // Returns a valid index to be selected after the tabs in `block_tabs` are
  // closed. If index is after the block, index is adjusted to reflect the fact
  // that the block is going away.
  int GetTabIndexAfterClosing(int index, const gfx::Range& block_tabs) const;

  // Takes the |selection| change and decides whether to forget the openers.
  void OnActiveTabChanged(const TabStripSelectionChange& selection);

  // Checks if policy allows a tab to be closed.
  bool PolicyAllowsTabClosing(content::WebContents* contents) const;

  // Determine where to shift selection after a tab or collection is closed.
  std::optional<int> DetermineNewSelectedIndex(
      std::variant<tabs::TabInterface*, tabs::TabCollection*> tab_or_collection)
      const;

  std::vector<std::pair<tabs::TabInterface*, int>> GetTabsAndIndicesInSplit(
      split_tabs::SplitTabId split_id);

  // If inserting at `index` breaks a split, returns its id, otherwise nullopt.
  std::optional<split_tabs::SplitTabId> InsertionBreaksSplitContiguity(
      int index);

  // Helper to determine if moving a block of tabs from `start_index` with block
  // size `length` to `final_index` breaks contiguity.
  std::optional<split_tabs::SplitTabId>
  MoveBreaksSplitContiguity(int start_index, int length, int final_index);

  void MaybeRemoveSplitsForMove(
      int initial_index,
      int final_index,
      const std::optional<tab_groups::TabGroupId> group,
      bool pin);

  // The WebContents data currently hosted within this TabStripModel. This must
  // be kept in sync with |selection_model_|.
  std::unique_ptr<tabs::TabStripCollection> contents_data_;

  // The model for tab groups hosted within this TabStripModel.
  std::unique_ptr<TabGroupModel> group_model_;

  raw_ptr<TabStripModelDelegate> delegate_;

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
  // Deleting the constructor gives a clear error message traceable back to
  // here.
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
