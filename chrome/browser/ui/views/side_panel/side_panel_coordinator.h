// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class BrowserView;
class SidePanelComboboxModel;

namespace views {
class ImageButton;
class Combobox;
class View;
}  // namespace views

// Class used to manage the state of side-panel content. Clients should manage
// side-panel visibility using this class rather than explicitly showing/hiding
// the side-panel View.
// This class is also responsible for consolidating multiple SidePanelEntry
// classes across multiple SidePanelRegistry instances, potentially merging them
// into a single unified side panel.
// Existence and value of registries' active_entry() determines which entry is
// visible for a given tab where the order of precedence is contextual
// registry's active_entry() then global registry's. These values are reset when
// the side panel is closed and |last_active_global_entry_id_| is used to
// determine what entry is seen when the panel is reopened.
class SidePanelCoordinator final : public SidePanelRegistryObserver,
                                   public TabStripModelObserver {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void Show(absl::optional<SidePanelEntry::Id> entry_id = absl::nullopt,
            absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                absl::nullopt);
  void Show(SidePanelEntry::Key entry_key,
            absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                absl::nullopt);
  void Close();
  void Toggle();

  // Opens the current side panel contents in a new tab. This is called by the
  // header button, when it's visible.
  void OpenInNewTab();

  SidePanelRegistry* GetGlobalSidePanelRegistry();

  // Prevent content swapping delays from happening for testing.
  // This should be called before the side panel is first shown.
  void SetNoDelaysForTesting();

  SidePanelEntry* GetCurrentSidePanelEntryForTesting() {
    return current_entry_.get();
  }

  views::Combobox* GetComboboxForTesting() { return header_combobox_; }

  SidePanelComboboxModel* GetComboboxModelForTesting() {
    return combobox_model_.get();
  }

  absl::optional<SidePanelEntry::Id> GetCurrentEntryId() const;

  SidePanelEntry::Id GetComboboxDisplayedEntryIdForTesting() const;

  SidePanelEntry* GetLoadingEntryForTesting() const;

  bool IsSidePanelShowing();

  // Re-runs open new tab URL check and sets button state to enabled/disabled
  // accordingly.
  void UpdateNewTabButtonState();

  void AddSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

  void RemoveSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

 private:
  friend class SidePanelCoordinatorTest;
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           ShowEmptyUserNoteSidePanel);
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           PopulateUserNoteSidePanel);

  views::View* GetContentView() const;
  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key);

  void SetSidePanelButtonTooltipText(std::u16string tooltip_text);

  // Creates header and SidePanelEntry content container within the side panel.
  void InitializeSidePanel();

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry and
  // |content_view| if provided, otherwise get the content_view from the
  // provided SidePanelEntry.
  void PopulateSidePanel(
      SidePanelEntry* entry,
      absl::optional<std::unique_ptr<views::View>> content_view);

  // Clear cached views for registry entries for global and contextual
  // registries.
  void ClearCachedEntryViews();

  // Returns the last active entry or the reading list entry if no last active
  // entry exists.
  absl::optional<SidePanelEntry::Key> GetLastActiveEntryKey() const;

  // Returns the currently selected id in the combobox, if one is shown.
  absl::optional<SidePanelEntry::Key> GetSelectedKey() const;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  std::unique_ptr<views::View> CreateHeader();
  std::unique_ptr<views::Combobox> CreateCombobox();

  // This is called after a user has made a selection in the combobox dropdown
  // and before any selected id and combobox model change takes place. This
  // allows us to make the entry displayed in the combobox follow the same
  // delays as the side panel content when there are delays for loading content.
  bool OnComboboxChangeTriggered(size_t index);

  // SidePanelRegistryObserver:
  void OnEntryRegistered(SidePanelEntry* entry) override;
  void OnEntryWillDeregister(SidePanelEntry* entry) override;
  void OnEntryIconUpdated(SidePanelEntry* entry) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // When true, prevent loading delays when switching between side panel
  // entries.
  bool no_delays_for_testing_ = false;

  // Timestamp of when the side panel was opened. Updated when the side panel is
  // triggered to be opened, not when visibility changes. These can differ due
  // to delays for loading content. This is used for metrics.
  base::TimeTicks opened_timestamp_;

  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;
  raw_ptr<SidePanelRegistry> global_registry_;
  absl::optional<SidePanelEntry::Key> last_active_global_entry_key_;

  // current_entry_ tracks the entry that currently has its view hosted by the
  // side panel. It is necessary as current_entry_ may belong to a contextual
  // registry that is swapped out (during a tab switch for e.g.). In such
  // situations we may still need a reference to the entry corresponding to the
  // hosted view so we can cache and clean up appropriately before switching in
  // the new entry.
  // Use a weak pointer so that current side panel entry can be reset
  // automatically if the entry is destroyed.
  base::WeakPtr<SidePanelEntry> current_entry_;

  // Used to update SidePanelEntry options in the header_combobox_ based on
  // their availability in the observed side panel registries.
  std::unique_ptr<SidePanelComboboxModel> combobox_model_;
  raw_ptr<views::Combobox, DanglingUntriaged> header_combobox_ = nullptr;

  // Used to update the visibility of the 'Open in New Tab' header button.
  raw_ptr<views::ImageButton, DanglingUntriaged>
      header_open_in_new_tab_button_ = nullptr;

  base::ObserverList<SidePanelViewStateObserver> view_state_observers_;

  // TODO(pbos): Add awareness of tab registries here. This probably needs to
  // know the tab registry it's currently monitoring.
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
