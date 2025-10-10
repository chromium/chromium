// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_observer.h"

class BrowserView;

namespace views {
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
// registry's active_entry() then global registry's.
class SidePanelCoordinator final
    : public SidePanelUIBase,
      public views::ViewObserver,
      public SidePanelToolbarPinningController::Observer {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void Init(Browser* browser);
  void TearDownPreBrowserWindowDestruction();

  // SidePanelUI:
  void Close() override;
  void Toggle(SidePanelEntryKey key,
              SidePanelUtil::SidePanelOpenTrigger open_trigger) override;
  void OpenInNewTab() override;

  // Re-runs open new tab URL check and sets button state to enabled/disabled
  // accordingly.
  void UpdateNewTabButtonState();

  // SidePanelUIBase:
  using SidePanelUIBase::Show;
  void Close(bool suppress_animations) override;
  void Show(const UniqueKey& entry,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;

  // Register for this callback to detect when the side panel opens or changes.
  // If the open is animated, this will be called at the beginning of the
  // animation.
  using ShownCallback = base::RepeatingCallback<void()>;
  base::CallbackListSubscription RegisterSidePanelShown(ShownCallback callback);

  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;

  SidePanelEntry* GetCurrentSidePanelEntryForTesting();

  SidePanelEntry* GetLoadingEntryForTesting() const;

 private:
  friend class SidePanelCoordinatorTest;

  void UpdatePinState();

  // Returns the corresponding entry for `entry_key` or a nullptr if this key is
  // not registered in the currently observed registries. This looks through the
  // active contextual registry first, then the global registry.
  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key) const;

  // SidePanelUIBase:
  void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view) override;
  void MaybeShowEntryOnTabStripModelChanged(
      SidePanelRegistry* old_contextual_registry,
      SidePanelRegistry* new_contextual_registry) override;

  // Clear cached views with the corresponding panel type for registry entries
  // for global and contextual registries.
  void ClearCachedEntryViews(SidePanelEntry::PanelType type);

  void UpdateSidePanelHeader(SidePanelEntry* entry);

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_from,
                               bool visible) override;

  // Called when the action item associated with the side panel entry changes.
  // The key is the unique key of the action item that has changed.
  void OnActionItemChanged(UniqueKey key);

  void MaybeQueuePinPromo(SidePanelEntryId id);
  void ShowPinPromo();
  void MaybeEndPinPromo(bool pinned);

  // Opens the more info menu. This is called by the header button, when it's
  // visible.
  void OpenMoreInfoMenu();

  // SidePanelToolbarPinningController::Observer:
  void OnPinStateChanged() override;

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

  // This subscription is used to update the side panel title when the action
  // item associated with the side panel entry changes.
  base::CallbackListSubscription action_item_controller_subscription_;

  // Model for the more info menu.
  std::unique_ptr<ui::MenuModel> more_info_menu_model_;

  // Runner for the more info menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Provides delay on pinning promo.
  base::OneShotTimer pin_promo_timer_;

  // Set to the appropriate pin promo for the current side panel entry, or null
  // if none. (Not set if e.g. already pinned.)
  raw_ptr<const base::Feature> pending_pin_promo_ = nullptr;

  std::unique_ptr<SidePanelToolbarPinningController>
      side_panel_toolbar_pinning_controller_;

  base::ScopedObservation<SidePanelToolbarPinningController,
                          SidePanelToolbarPinningController::Observer>
      side_panel_toolbar_pinning_controller_observation_{this};

  base::RepeatingCallbackList<void()> shown_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
