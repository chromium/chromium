// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"

class BrowserView;
class SidePanelComboboxModel;

namespace views {
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

  void Show(absl::optional<SidePanelEntry::Id> entry_id = absl::nullopt);
  void Close();
  void Toggle();

  SidePanelRegistry* GetGlobalSidePanelRegistry();

 private:
  friend class SidePanelCoordinatorTest;

  views::View* GetContentView();
  SidePanelEntry* GetEntryForId(SidePanelEntry::Id entry_id);

  // Creates header and SidePanelEntry content container within the side panel.
  void InitializeSidePanel();

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry.
  void PopulateSidePanel(SidePanelEntry* entry);

  // Clear cached views for registry entries for global and contextual
  // registries.
  void ClearCachedEntryViews();

  // Returns the last active entry or the reading list entry if no last active
  // entry exists.
  absl::optional<SidePanelEntry::Id> GetLastActiveEntryId() const;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  std::unique_ptr<views::View> CreateHeader();
  std::unique_ptr<views::Combobox> CreateCombobox();
  void OnComboboxChanged();

  // SidePanelRegistryObserver:
  void OnEntryRegistered(SidePanelEntry* entry) override;
  void OnEntryWillDeregister(SidePanelEntry* entry) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  const raw_ptr<BrowserView> browser_view_;
  raw_ptr<SidePanelRegistry> global_registry_;
  absl::optional<SidePanelEntry::Id> last_active_global_entry_id_;

  // Used to update SidePanelEntry options in the header_combobox_ based on
  // their availability in the observed side panel registries.
  std::unique_ptr<SidePanelComboboxModel> combobox_model_;
  raw_ptr<views::Combobox> header_combobox_ = nullptr;

  // TODO(pbos): Add awareness of tab registries here. This probably needs to
  // know the tab registry it's currently monitoring.
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
