// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

class Browser;
class SidePanelEntry;
class SidePanelEntryWaiter;

namespace views {
class View;
}

// Base class for Side Panel UIs that contains the common logic for managing
// side panel entries and state.
class CHROME_VIEWS_EXPORT SidePanelUIBase : public SidePanelUI,
                                            public TabStripModelObserver {
 public:
  explicit SidePanelUIBase(Browser* browser);
  ~SidePanelUIBase() override;

  SidePanelUIBase(const SidePanelUIBase&) = delete;
  SidePanelUIBase& operator=(const SidePanelUIBase&) = delete;

  // The side panel entry to be shown is uniquely specified via a tuple:
  //  (tab or window-scoped registry, SidePanelEntry::Key). `tab_handle` is
  //  necessary since it's possible for a Key to be present in both the
  //  tab-scoped and window-scoped registry, or in multiple different tab-scoped
  //  registries.
  struct UniqueKey {
    std::optional<tabs::TabHandle> tab_handle;
    SidePanelEntry::Key key;
    friend bool operator==(const UniqueKey&, const UniqueKey&) = default;
  };

  // SidePanelUI:
  //
  // Use NOINLINE for these exported virtual overrides. On the one hand, it's
  // necessary to export this class in order to have it available across
  // component boundaries, but on the other, we need to prevent the methods from
  // being both locally defined and imported in some tests for component builds.
  using SidePanelUI::Show;
  NOINLINE void Show(
      SidePanelEntry::Id entry_id,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) override;
  NOINLINE void Show(
      SidePanelEntry::Key entry_key,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) override;
  NOINLINE std::optional<SidePanelEntry::Id> GetCurrentEntryId() const override;
  NOINLINE int GetCurrentEntryDefaultContentWidth() const override;
  NOINLINE bool IsSidePanelShowing() const override;
  NOINLINE bool IsSidePanelEntryShowing(
      const SidePanelEntry::Key& entry_key) const override;

  // Similar to IsSidePanelEntryShowing, but restricts to either the tab-scoped
  // or window-scoped registry.
  bool IsSidePanelEntryShowing(const SidePanelEntry::Key& entry_key,
                               bool for_tab) const;

  Browser* browser() const { return browser_; }
  SidePanelRegistry* GetWindowRegistry() { return window_registry_.get(); }

  std::optional<UniqueKey> current_key() const { return current_key_; }
  base::WeakPtr<SidePanelEntry> current_entry() const { return current_entry_; }

 protected:
  friend class SidePanelEntryWaiter;

  // This method does not show the side panel. Instead, it queues the side panel
  // to be shown once the contents have been loaded. This process may be either
  // synchronous or asynchronous.
  virtual void Show(
      const UniqueKey& entry,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
      bool suppress_animations) = 0;

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry and
  // `content_view` if provided, otherwise get the content_view from the
  // provided SidePanelEntry.
  virtual void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view) = 0;

  void set_current_key(std::optional<UniqueKey> new_key) {
    current_key_ = new_key;
  }

  void set_current_entry(base::WeakPtr<SidePanelEntry> new_entry) {
    current_entry_ = new_entry;
  }

  std::optional<UniqueKey> GetUniqueKeyForKey(
      const SidePanelEntry::Key& entry_key) const;

  // Returns the SidePanelEntry uniquely specified by UniqueKey.
  SidePanelEntry* GetEntryForUniqueKey(const UniqueKey& unique_key) const;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  SidePanelEntry* GetActiveContextualEntryForKey(
      const SidePanelEntry::Key& entry_key) const;

  const raw_ptr<Browser> browser_;

  // This registry is scoped to the browser window and is owned by this class.
  std::unique_ptr<SidePanelRegistry> window_registry_;

  // Inner class that waits for side panel entries to load.
  std::unique_ptr<SidePanelEntryWaiter> waiter_;

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // current_key_ uniquely identifies the SidePanelEntry that has its view
  // hosted by the side panel. At the time that it is set and for most code
  // paths, the SidePanelEntry is guaranteed to exist. It does not exist in the
  // following cases:
  //   * The active tab is switched, and UniqueKey is tab-scoped.
  //   * The entry is removed from tab or window-scoped registry.
  // The side-panel is showing if and only if current_key_ is set. That means it
  // must only be set in one place: PopulateSidePanel() and unset in one place:
  // OnViewVisibilityChanged()
  std::optional<UniqueKey> current_key_;

  // TODO(https://crbug.com/363743081): Remove this member.
  // There are a few cases where the current control flow first modifies the
  // active registry, then tries to reference the previous entry.
  base::WeakPtr<SidePanelEntry> current_entry_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_
