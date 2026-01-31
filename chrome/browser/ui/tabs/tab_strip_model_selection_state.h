// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_STATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_STATE_H_

#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/models/list_selection_model.h"

class TabStripModel;

namespace tabs {

class TabInterface;

// Stores the selection state for a TabStripModel. This is an internal
// implementation detail of TabStripModel and should not be used by other
// classes. It stores pointers to TabInterface objects, which are owned by
// TabStripModel's TabStripCollection.
class TabStripModelSelectionState final {
 public:
  explicit TabStripModelSelectionState(TabStripModel* model = nullptr);
  TabStripModelSelectionState(
      std::unordered_set<raw_ptr<TabInterface>> selected_tabs,
      raw_ptr<TabInterface> active_tab,
      raw_ptr<TabInterface> anchor_tab);
  TabStripModelSelectionState(const TabStripModelSelectionState&);
  TabStripModelSelectionState& operator=(const TabStripModelSelectionState&);
  ~TabStripModelSelectionState();

  bool operator==(const TabStripModelSelectionState& other) const;

  TabInterface* active_tab() const { return active_tab_; }
  TabInterface* anchor_tab() const { return anchor_tab_; }
  const std::unordered_set<raw_ptr<TabInterface>>& selected_tabs() const {
    return selected_tabs_;
  }

  void Clear();
  bool empty() const { return selected_tabs_.empty(); }
  size_t size() const { return selected_tabs_.size(); }

  // Returns whether the tab is in the selected_tabs_.
  bool IsSelected(TabInterface* tab) const;

  // Adds the tab to the selected_tabs_ if `tab` is non-null.
  void AddTabToSelection(TabInterface* tab);

  // Removes tabs from the selected_tabs_ if `tab` is non-null.
  void RemoveTabFromSelection(TabInterface* tab);

  // Sets the active tab to the given tab interface. If that tab is not part of
  // the selection model, then it's added to the list of selected tabs.
  void SetActiveTab(TabInterface* tab);

  // Sets the anchor tab to the given tab interface. If that tab is not part of
  // the selection model, then it's added to the list of selected tabs.
  void SetAnchorTab(TabInterface* tab);

  // Adds tabs to the selection model, does not update the active tab, or the
  // anchor tab.
  bool AppendTabsToSelection(const std::unordered_set<TabInterface*>& tabs);

  // Updates the set of selected tabs with the new TabInterface ptrs. If the
  // active/anchor tabs are provided, then they will be CHECKed to make sure
  // they're part of the new list of tabs. If not provided, then the active tab
  // and anchor tab will be set to the "first" tab in the set.
  void SetSelectedTabs(std::unordered_set<TabInterface*> tabs,
                       TabInterface* active_tab = nullptr,
                       TabInterface* anchor_tab = nullptr);

  // Returns true if the selection model has at least 1 selected tab, an anchor
  // and an active tab. Otherwise returns false.
  bool Valid() const;

  // Access the current object as a ListSelectionModel. Requires the
  // |model_| member to be non null.
  const ui::ListSelectionModel& GetListSelectionModel() const;

  // Helper functions to update the |list_selection_model_| member,
  // which is a ListSelectionModel representation of the current
  // selection model. Requires |model_| to be non null.
  void InvalidateListSelectionModel(base::PassKey<TabStripModel>) const;
  void UpdateListSelectionModel(base::PassKey<TabStripModel>) const;

 private:
  void UpdateListSelectionModel() const;
  ui::ListSelectionModel::SelectedIndices ComputeSelectedIndices() const;
  void InvalidateListSelectionModel() const;

  // The selected tabs in the tabstrip.
  std::unordered_set<raw_ptr<TabInterface>> selected_tabs_;

  // The active tab.
  raw_ptr<TabInterface> active_tab_ = nullptr;

  // The anchor tab for selection.
  raw_ptr<TabInterface> anchor_tab_ = nullptr;

  // If |model_| is non-null, this member represents a ListSelectionModel that
  // represents the current selection model. Otherwise it is a nullopt. The
  // value of this member is cached and generated only on access, if
  // necessary.
  mutable std::optional<ui::ListSelectionModel> list_selection_model_;

  raw_ptr<TabStripModel> model_ = nullptr;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_STATE_H_
