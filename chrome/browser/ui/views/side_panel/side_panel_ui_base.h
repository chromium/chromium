// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
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
class SidePanelUIBase : public SidePanelUI, public TabStripModelObserver {
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
  using SidePanelUI::Close;
  using SidePanelUI::Show;
  void Show(SidePanelEntry::Id entry_id,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;
  void Show(SidePanelEntry::Key entry_key,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;
  std::optional<SidePanelEntry::Id> GetCurrentEntryId(
      SidePanelEntry::PanelType panel_type) const override;
  int GetCurrentEntryDefaultContentWidth(
      SidePanelEntry::PanelType type) const override;
  bool IsSidePanelShowing(SidePanelEntry::PanelType type) const override;
  bool IsSidePanelEntryShowing(
      const SidePanelEntry::Key& entry_key) const override;
  bool IsSidePanelEntryShowing(const SidePanelEntry::Key& entry_key,
                               bool for_tab) const override;
  base::CallbackListSubscription RegisterSidePanelShown(
      SidePanelEntry::PanelType type,
      SidePanelUI::ShownCallback callback) override;

  Browser* browser() const { return browser_; }

 protected:
  friend class SidePanelEntryWaiter;

  struct PanelData {
    PanelData();
    ~PanelData();

    // current_key_ uniquely identifies the SidePanelEntry that has its view
    // hosted by the side panel. At the time that it is set and for most code
    // paths, the SidePanelEntry is guaranteed to exist. It does not exist in
    // the following cases:
    //   * The active tab is switched, and UniqueKey is tab-scoped.
    //   * The entry is removed from tab or window-scoped registry.
    // The side-panel is showing if and only if current_key_ is set. That means
    // it must only be set in one place: PopulateSidePanel() and unset in one
    // place: OnViewVisibilityChanged()
    std::optional<SidePanelUIBase::UniqueKey> current_key = std::nullopt;

    // Inner class that waits for side panel entries to load.
    std::unique_ptr<SidePanelEntryWaiter> waiter;

    // Timestamp of when the side panel was opened. Updated when the side panel
    // is triggered to be opened, not when visibility changes. These can differ
    // due to delays for loading content. This is used for metrics.
    base::TimeTicks opened_timestamp;

    // Callback list notified when the side panel opens or changes.
    base::RepeatingCallbackList<void()> shown_callback_list;
  };

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

  // Shows an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  virtual void MaybeShowEntryOnTabStripModelChanged(
      SidePanelRegistry* old_contextual_registry,
      SidePanelRegistry* new_contextual_registry) = 0;

  void SetOpenedTimestamp(SidePanelEntry::PanelType type,
                          base::TimeTicks timestamp);
  base::TimeTicks opened_timestamp(SidePanelEntry::PanelType type) {
    return panel_data_.at(type)->opened_timestamp;
  }

  void NotifyShownCallbacksFor(SidePanelEntry::PanelType type);

  std::optional<UniqueKey> current_key(SidePanelEntry::PanelType type) const {
    return panel_data_.at(type)->current_key;
  }
  void SetCurrentKey(SidePanelEntry::PanelType type,
                     std::optional<UniqueKey> new_key);

  std::optional<UniqueKey> GetUniqueKeyForKey(
      const SidePanelEntry::Key& entry_key) const;

  // Returns the SidePanelEntry uniquely specified by UniqueKey.
  SidePanelEntry* GetEntryForUniqueKey(const UniqueKey& unique_key) const;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  SidePanelEntry* GetActiveContextualEntryForKey(
      const SidePanelEntry::Key& entry_key) const;

  // Returns the new entry key to be shown after the active tab has changed, or
  // nullopt if no suitable entry is found. Called from
  // `OnTabStripModelChanged()` when there's an active entry being shown in the
  // side panel.
  std::optional<UniqueKey> GetNewActiveKeyOnTabChanged(
      SidePanelEntry::PanelType type);

  SidePanelEntryWaiter* waiter(SidePanelEntry::PanelType type) const;

  const raw_ptr<Browser> browser_;

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  std::map<SidePanelEntry::PanelType, std::unique_ptr<PanelData>> panel_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UI_BASE_H_
