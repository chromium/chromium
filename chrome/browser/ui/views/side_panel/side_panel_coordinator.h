// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_observer.h"

class BrowserView;

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ImageButton;
class MenuRunner;
class ToggleImageButton;
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
class SidePanelCoordinator final : public TabStripModelObserver,
                                   public views::ViewObserver,
                                   public PinnedToolbarActionsModel::Observer,
                                   public SidePanelUI,
                                   public ToolbarActionsModel::Observer {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void Init(Browser* browser);
  void TearDownPreBrowserViewDestruction();

  SidePanelRegistry* GetWindowRegistry();

  // SidePanelUI:
  void Show(SidePanelEntry::Id entry_id,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                std::nullopt) override;
  void Show(SidePanelEntry::Key entry_key,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                std::nullopt) override;
  void Close() override;
  void Toggle(SidePanelEntryKey key,
              SidePanelUtil::SidePanelOpenTrigger open_trigger) override;
  void OpenInNewTab() override;
  void UpdatePinState() override;
  std::optional<SidePanelEntry::Id> GetCurrentEntryId() const override;
  bool IsSidePanelShowing() const override;
  bool IsSidePanelEntryShowing(
      const SidePanelEntry::Key& entry_key) const override;
  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  // Returns the web contents in a side panel if one exists.
  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;

  // Similar to IsSidePanelEntryShowing, but restricts to either the tab-scoped
  // or window-scoped registry.
  bool IsSidePanelEntryShowing(const SidePanelEntry::Key& entry_key,
                               bool for_tab) const;

  // Re-runs open new tab URL check and sets button state to enabled/disabled
  // accordingly.
  void UpdateNewTabButtonState();

  void UpdateHeaderPinButtonState();

  SidePanelEntry* GetCurrentSidePanelEntryForTesting();

  actions::ActionItem* GetActionItem(SidePanelEntry::Key entry_key);

  views::ToggleImageButton* GetHeaderPinButtonForTesting() {
    return header_pin_button_;
  }

  views::ImageButton* GetHeaderMoreInfoButtonForTesting() {
    return header_more_info_button_;
  }

  SidePanelEntry* GetLoadingEntryForTesting() const;

  void AddSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

  void RemoveSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

  void Close(bool suppress_animations);

  // TODO(https://crbug.com/363743081): This method should be removed and the
  // logic moved to ExtensionSidePanelCoordinator.
  void OnEntryWillDeregister(SidePanelRegistry* registry,
                             SidePanelEntry* entry);

  // The side panel entry to be shown is uniquely specified via a tuple:
  //  (tab or window-scoped registry, SidePanelEntry::Key). `tab_handle` is
  //  necessary since it's possible for a Key to be present in both the
  //  tab-scoped and window-scoped registry, or in multiple different tab-scoped
  //  registries.
  struct UniqueKey {
    std::optional<uint32_t> tab_handle;
    SidePanelEntry::Key key;
    friend bool operator==(const UniqueKey&, const UniqueKey&) = default;
  };

  // This method does not show the side panel. Instead, it queues the side panel
  // to be shown once the contents has been loaded. This process may be either
  // synchronous or asynchronous.
  void Show(const UniqueKey& entry,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations);

  std::optional<UniqueKey> current_key() { return current_key_; }

 private:
  friend class SidePanelCoordinatorTest;
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           ShowEmptyUserNoteSidePanel);
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           PopulateUserNoteSidePanel);

  void OnClosed();

  // Returns the corresponding entry for `entry_key` or a nullptr if this key is
  // not registered in the currently observed registries. This looks through the
  // active contextual registry first, then the global registry.
  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key) const;
  std::optional<UniqueKey> GetUniqueKeyForKey(
      const SidePanelEntry::Key& entry_key) const;

  SidePanelEntry* GetActiveContextualEntryForKey(
      const SidePanelEntry::Key& entry_key) const;

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry and
  // `content_view` if provided, otherwise get the content_view from the
  // provided SidePanelEntry.
  void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view);

  // Clear cached views for registry entries for global and contextual
  // registries.
  void ClearCachedEntryViews();

  void UpdatePanelIconAndTitle(const ui::ImageModel& icon,
                               const std::u16string& text,
                               const bool should_show_title_text,
                               const bool is_extension);

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_from) override;

  // PinnedToolbarActionsModel::Observer:
  void OnActionAddedLocally(const actions::ActionId& id) override {}
  void OnActionRemovedLocally(const actions::ActionId& id) override {}
  void OnActionMovedLocally(const actions::ActionId& id,
                            int from_index,
                            int to_index) override {}
  void OnActionsChanged() override;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  std::unique_ptr<views::View> CreateHeader();

  // Returns the new entry to be shown after the active entry is deregistered,
  // or nullopt if no suitable entry is found. Called from
  // `OnEntryWillDeregister()` when there's an active entry being shown in the
  // side panel.
  std::optional<UniqueKey> GetNewActiveKeyOnDeregister(
      SidePanelRegistry* deregistering_registry,
      const SidePanelEntry::Key& key);

  // Returns the new entry key to be shown after the active tab has changed, or
  // nullopt if no suitable entry is found. Called from
  // `OnTabStripModelChanged()` when there's an active entry being shown in the
  // side panel.
  std::optional<UniqueKey> GetNewActiveKeyOnTabChanged();

  void NotifyPinnedContainerOfActiveStateChange(SidePanelEntryKey key,
                                                bool is_active);

  void MaybeQueuePinPromo();
  void ShowPinPromo();
  void MaybeEndPinPromo(bool pinned);

  // Opens the more info menu. This is called by the header button, when it's
  // visible.
  void OpenMoreInfoMenu();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarModelInitialized() override {}
  void OnToolbarPinnedActionsChanged() override;

  // Returns the SidePanelEntry uniquely specified by UniqueKey.
  SidePanelEntry* GetEntryForUniqueKey(const UniqueKey& unique_key) const;

  // Closes `promo_feature` if showing and if actual_id == promo_id, also
  // notifies the User Education system that the feature was used.
  void ClosePromoAndMaybeNotifyUsed(const base::Feature& promo_feature,
                                    SidePanelEntryId promo_id,
                                    SidePanelEntryId actual_id);

  // Timestamp of when the side panel was opened. Updated when the side panel is
  // triggered to be opened, not when visibility changes. These can differ due
  // to delays for loading content. This is used for metrics.
  base::TimeTicks opened_timestamp_;

  const raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_;

  // This registry is scoped to the browser window and is owned by this class.
  std::unique_ptr<SidePanelRegistry> window_registry_;

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

  // Used to update icon in the side panel header.
  raw_ptr<views::ImageView, AcrossTasksDanglingUntriaged> panel_icon_ = nullptr;

  // Used to update the displayed title in the side panel header.
  raw_ptr<views::Label, AcrossTasksDanglingUntriaged> panel_title_ = nullptr;

  // Used to update the visibility of the 'Open in New Tab' header button.
  raw_ptr<views::ImageButton, AcrossTasksDanglingUntriaged>
      header_open_in_new_tab_button_ = nullptr;

  // Used to update the visibility of the pin header button.
  raw_ptr<views::ToggleImageButton, AcrossTasksDanglingUntriaged>
      header_pin_button_ = nullptr;

  // Used to update the visibility of the more info button.
  raw_ptr<views::ImageButton, AcrossTasksDanglingUntriaged>
      header_more_info_button_ = nullptr;

  // Model for the more info menu.
  std::unique_ptr<ui::MenuModel> more_info_menu_model_;

  // Runner for the more info menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Provides delay on pinning promo.
  base::OneShotTimer pin_promo_timer_;

  // Inner class that waits for side panel entries to load.
  class SidePanelEntryWaiter;
  std::unique_ptr<SidePanelEntryWaiter> waiter_;

  // Set to the appropriate pin promo for the current side panel entry, or null
  // if none. (Not set if e.g. already pinned.)
  raw_ptr<const base::Feature> pending_pin_promo_ = nullptr;

  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      extensions_model_observation_{this};

  base::ObserverList<SidePanelViewStateObserver> view_state_observers_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_model_observation_{this};
};

namespace base {

// Since SidePanelCoordinator defines custom method names to add and remove
// observers, we need define a new trait customization to use
// `base::ScopedObservation` and `base::ScopedMultiSourceObservation`.
// See `base/scoped_observation_traits.h` for more details.
template <>
struct ScopedObservationTraits<SidePanelCoordinator,
                               SidePanelViewStateObserver> {
  static void AddObserver(SidePanelCoordinator* source,
                          SidePanelViewStateObserver* observer) {
    source->AddSidePanelViewStateObserver(observer);
  }
  static void RemoveObserver(SidePanelCoordinator* source,
                             SidePanelViewStateObserver* observer) {
    source->RemoveSidePanelViewStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
